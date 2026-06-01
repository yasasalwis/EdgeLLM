#include "LLMClient.h"

#include <ArduinoJson.h>

namespace edge {

namespace {
// Allocation-light integer formatting (std::to_string is unavailable on some
// embedded cores).
std::string itos(long v) {
  if (v == 0) return "0";
  bool neg = v < 0;
  unsigned long u = neg ? static_cast<unsigned long>(-v) : static_cast<unsigned long>(v);
  char buf[24];
  size_t i = sizeof(buf);
  while (u > 0 && i > 0) {
    buf[--i] = static_cast<char>('0' + (u % 10));
    u /= 10;
  }
  std::string s = neg ? "-" : "";
  s.append(buf + i, sizeof(buf) - i);
  return s;
}

// Best-effort extraction of a human-readable error from an API error body.
std::string extractErrorMessage(const std::string& body) {
  if (body.empty()) return "";
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    return body.size() > 200 ? body.substr(0, 200) : body;
  }
  const char* m = doc["error"]["message"];
  if (m) return m;
  const char* e = doc["error"];
  if (e) return e;
  const char* top = doc["message"];
  if (top) return top;
  return body.size() > 200 ? body.substr(0, 200) : body;
}
}  // namespace

LLMClient::LLMClient(Provider& provider, IConnection& conn, Logger* logger)
    : provider_(provider), conn_(conn), logger_(logger) {}

void LLMClient::configure(HttpClient& http) const {
  http.setClock(clock_);
  http.setTimeout(timeoutMs_);
  http.setMaxResponseBody(maxResponseBody_);
}

Error LLMClient::mapStatus(int httpStatus) {
  switch (httpStatus) {
    case 400: return Error::InvalidArgument;
    case 401: return Error::Unauthorized;
    case 403: return Error::Forbidden;
    case 404: return Error::NotFound;
    case 429: return Error::RateLimited;
    default: return Error::ProviderError;
  }
}

Result<StructuredResult> LLMClient::doGenerate(const ResponseSchema& schema,
                                               const MessageList& messages,
                                               const ChatOptions& opts) {
  Error lastError = Error::SchemaValidationFailed;
  const int attempts = static_cast<int>(structuredRetries_) + 1;
  for (int attempt = 0; attempt < attempts; ++attempt) {
    HttpRequest req;
    Status bs = provider_.buildStructuredRequest(messages, opts, schema, req);
    if (!bs) return Result<StructuredResult>::fail(bs.error());

    HttpClient http(conn_, logger_);
    configure(http);
    HttpResponse resp;
    Status s = http.send(req, resp);
    conn_.stop();

    // Transport / HTTP errors are not retried — retrying an auth or rate-limit
    // failure is pointless.
    if (!s) {
      if (logger_)
        logger_->warn(std::string(provider_.name()) + " request failed: " + s.message());
      return Result<StructuredResult>::fail(s.error());
    }
    if (!resp.isSuccess()) {
      if (logger_)
        logger_->warn(std::string(provider_.name()) + " http " + itos(resp.status) + ": " +
                      extractErrorMessage(resp.body));
      return Result<StructuredResult>::fail(mapStatus(resp.status));
    }

    std::string json;
    ChatResult meta;
    Status ps = provider_.parseStructuredResponse(resp, json, meta);
    if (!ps) {
      lastError = ps.error();
      continue;  // malformed response — retry
    }
    Status v = schema.validate(json);
    if (v.isOk()) return Result<StructuredResult>::ok(StructuredResult(std::move(json)));

    lastError = Error::SchemaValidationFailed;
    if (logger_)
      logger_->warn(std::string(provider_.name()) +
                    " output failed schema validation; retrying if attempts remain");
  }
  return Result<StructuredResult>::fail(lastError);
}

Result<StructuredResult> LLMClient::generate(const ResponseSchema& schema,
                                             const std::string& system, const std::string& user) {
  ChatOptions o = options_;
  if (!system.empty()) o.system = system;
  MessageList m{Message::user(user)};
  return doGenerate(schema, m, o);
}

Result<StructuredResult> LLMClient::generate(const ResponseSchema& schema,
                                             const MessageList& messages) {
  return doGenerate(schema, messages, options_);
}

Result<StructuredResult> LLMClient::generate(const ResponseSchema& schema, Conversation& convo) {
  ChatOptions o = options_;
  if (!convo.system().empty()) o.system = convo.system();
  Result<StructuredResult> r = doGenerate(schema, convo.messages(), o);
  if (r.isOk()) convo.addAssistant(r.value().json());
  return r;
}

namespace {
// Flattens an agent transcript (with tool-call turns and tool results) into a
// plain system/user/assistant message list for the final structured call, so
// providers don't need to serialize tool protocol in buildStructuredRequest.
MessageList flattenForStructured(const MessageList& working) {
  MessageList flat;
  flat.reserve(working.size());
  for (const auto& m : working) {
    if (m.role == Role::Tool) {
      flat.push_back(Message::user("[tool " + m.toolName + " result] " + m.content));
    } else if (m.role == Role::Assistant && !m.toolCalls.empty()) {
      flat.push_back(Message::assistant(m.content.empty() ? "(requested tools)" : m.content));
    } else {
      flat.push_back(m);
    }
  }
  return flat;
}
}  // namespace

Result<StructuredResult> LLMClient::run(const ResponseSchema& schema, const MessageList& messages,
                                        const ToolRegistry& tools) {
  if (!provider_.supportsTools()) {
    if (logger_)
      logger_->warn(std::string(provider_.name()) + " does not support tool calling");
    return Result<StructuredResult>::fail(Error::NotImplemented);
  }

  MessageList working = messages;
  bool settled = false;
  for (uint8_t iter = 0; iter < agentOptions_.maxIterations && !settled; ++iter) {
    HttpRequest req;
    Status bs = provider_.buildToolRequest(working, options_, tools, req);
    if (!bs) return Result<StructuredResult>::fail(bs.error());

    HttpClient http(conn_, logger_);
    configure(http);
    HttpResponse resp;
    Status s = http.send(req, resp);
    conn_.stop();
    if (!s) return Result<StructuredResult>::fail(s.error());
    if (!resp.isSuccess()) {
      if (logger_)
        logger_->warn(std::string(provider_.name()) + " http " + itos(resp.status) + ": " +
                      extractErrorMessage(resp.body));
      return Result<StructuredResult>::fail(mapStatus(resp.status));
    }

    AgentTurn turn;
    Status ps = provider_.parseToolResponse(resp, turn);
    if (!ps) return Result<StructuredResult>::fail(ps.error());

    if (!turn.wantsTools()) {
      settled = true;
      break;
    }

    Message assistantTurn(Role::Assistant, turn.text);
    assistantTurn.toolCalls = turn.toolCalls;
    working.push_back(std::move(assistantTurn));
    for (const auto& call : turn.toolCalls) {
      Result<ToolResult> dr = tools.dispatch(call.name, call.argumentsJson);
      ToolResult tr = dr.isOk() ? dr.value()
                                : ToolResult::error(std::string("tool '") + call.name +
                                                    "' failed: " + errorString(dr.error()));
      if (logger_)
        logger_->info(std::string("tool ") + call.name + (tr.isError ? " -> error" : " -> ok"));
      working.push_back(Message::toolResult(call, tr));
    }
  }

  if (!settled) {
    if (logger_) logger_->warn("agent loop reached max iterations");
    return Result<StructuredResult>::fail(Error::ToolIterationLimit);
  }

  // Produce the final answer as validated structured output.
  return doGenerate(schema, flattenForStructured(working), options_);
}

Result<StructuredResult> LLMClient::run(const ResponseSchema& schema, const std::string& prompt,
                                        const ToolRegistry& tools) {
  MessageList m{Message::user(prompt)};
  return run(schema, m, tools);
}

}  // namespace edge
