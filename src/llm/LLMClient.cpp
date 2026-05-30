#include "LLMClient.h"

#include <ArduinoJson.h>

#include "../transport/SseParser.h"

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
    return body.size() > 200 ? body.substr(0, 200) : body;  // not JSON; truncate
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

Result<ChatResult> LLMClient::doChat(const MessageList& messages, const ChatOptions& opts) {
  HttpRequest req;
  Status bs = provider_.buildChatRequest(messages, opts, false, req);
  if (!bs) return Result<ChatResult>::fail(bs.error());

  HttpClient http(conn_, logger_);
  configure(http);
  HttpResponse resp;
  Status s = http.send(req, resp);
  conn_.stop();

  if (!s) {
    if (logger_)
      logger_->warn(std::string(provider_.name()) + " request failed: " + s.message());
    return Result<ChatResult>::fail(s.error());
  }
  if (!resp.isSuccess()) {
    if (logger_)
      logger_->warn(std::string(provider_.name()) + " http " + itos(resp.status) + ": " +
                    extractErrorMessage(resp.body));
    return Result<ChatResult>::fail(mapStatus(resp.status));
  }

  ChatResult cr;
  Status ps = provider_.parseChatResponse(resp, cr);
  if (!ps) return Result<ChatResult>::fail(ps.error());
  return Result<ChatResult>::ok(cr);
}

Status LLMClient::doChatStream(const MessageList& messages, const ChatOptions& opts,
                               const DeltaFn& onDelta, ChatResult& out) {
  HttpRequest req;
  Status bs = provider_.buildChatRequest(messages, opts, true, req);
  if (!bs) return bs;

  HttpClient http(conn_, logger_);
  configure(http);
  HttpResponse headers;

  const StreamFormat fmt = provider_.streamFormat();
  SseParser sse;
  std::string ndBuf;   // NDJSON line accumulator
  std::string errBuf;  // captured error body when status != 2xx
  bool done = false;

  auto handlePayload = [&](const std::string& payload) {
    StreamDelta d;
    Status ps = provider_.parseStreamEvent(payload, d);
    if (!ps) return;  // skip an unparseable keep-alive/partial; do not abort
    if (!d.textDelta.empty()) {
      out.text += d.textDelta;
      if (onDelta) onDelta(d.textDelta);
    }
    if (!d.finishReason.empty()) out.finishReason = d.finishReason;
    if (d.inputTokens) out.inputTokens = d.inputTokens;
    if (d.outputTokens) out.outputTokens = d.outputTokens;
    if (d.done) done = true;
  };

  HttpClient::BodyChunkFn onChunk = [&](const char* data, size_t len) -> bool {
    if (!headers.isSuccess()) {
      if (errBuf.size() < 8192) errBuf.append(data, len);  // bounded capture
      return true;
    }
    if (fmt == StreamFormat::SSE) {
      sse.feed(data, len, [&](const SseEvent& ev) { handlePayload(ev.data); });
    } else {
      ndBuf.append(data, len);
      size_t nl;
      while ((nl = ndBuf.find('\n')) != std::string::npos) {
        std::string line = ndBuf.substr(0, nl);
        ndBuf.erase(0, nl + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) handlePayload(line);
      }
    }
    return !done;  // stop reading once the stream signals completion
  };

  Status s = http.sendStream(req, headers, onChunk);

  // Flush a trailing NDJSON line that had no terminating newline.
  if (fmt == StreamFormat::NDJSON && !done && !ndBuf.empty()) {
    if (ndBuf.back() == '\r') ndBuf.pop_back();
    if (!ndBuf.empty()) handlePayload(ndBuf);
  }
  conn_.stop();

  if (!headers.isSuccess() && headers.status != 0) {
    if (logger_)
      logger_->warn(std::string(provider_.name()) + " http " + itos(headers.status) + ": " +
                    extractErrorMessage(errBuf));
    return Status::fail(mapStatus(headers.status));
  }
  if (!s) {
    if (logger_)
      logger_->warn(std::string(provider_.name()) + " stream failed: " + s.message());
    return s;
  }
  return Status::ok();
}

// --- Blocking ---

Result<ChatResult> LLMClient::chat(const std::string& userText) {
  MessageList m{Message::user(userText)};
  return doChat(m, options_);
}

Result<ChatResult> LLMClient::chat(const MessageList& messages) { return doChat(messages, options_); }

Result<ChatResult> LLMClient::chat(Conversation& convo) {
  ChatOptions o = options_;
  if (!convo.system().empty()) o.system = convo.system();
  Result<ChatResult> r = doChat(convo.messages(), o);
  if (r.isOk()) convo.addAssistant(r.value().text);
  return r;
}

// --- Streaming ---

Status LLMClient::chatStream(const std::string& userText, const DeltaFn& onDelta,
                             ChatResult* finalOut) {
  MessageList m{Message::user(userText)};
  ChatResult tmp;
  Status s = doChatStream(m, options_, onDelta, tmp);
  if (finalOut) *finalOut = tmp;
  return s;
}

Status LLMClient::chatStream(const MessageList& messages, const DeltaFn& onDelta,
                             ChatResult* finalOut) {
  ChatResult tmp;
  Status s = doChatStream(messages, options_, onDelta, tmp);
  if (finalOut) *finalOut = tmp;
  return s;
}

Status LLMClient::chatStream(Conversation& convo, const DeltaFn& onDelta, ChatResult* finalOut) {
  ChatOptions o = options_;
  if (!convo.system().empty()) o.system = convo.system();
  ChatResult tmp;
  Status s = doChatStream(convo.messages(), o, onDelta, tmp);
  if (s.isOk()) convo.addAssistant(tmp.text);
  if (finalOut) *finalOut = tmp;
  return s;
}

// --- Agent loop ---

Result<ChatResult> LLMClient::doRun(MessageList messages, const ChatOptions& opts,
                                    const ToolRegistry& tools) {
  if (!provider_.supportsTools()) {
    if (logger_)
      logger_->warn(std::string(provider_.name()) + " does not support tool calling yet");
    return Result<ChatResult>::fail(Error::NotImplemented);
  }

  ChatResult agg;
  for (uint8_t iter = 0; iter < agentOptions_.maxIterations; ++iter) {
    HttpRequest req;
    Status bs = provider_.buildToolRequest(messages, opts, tools, req);
    if (!bs) return Result<ChatResult>::fail(bs.error());

    HttpClient http(conn_, logger_);
    configure(http);
    HttpResponse resp;
    Status s = http.send(req, resp);
    conn_.stop();
    if (!s) {
      if (logger_)
        logger_->warn(std::string(provider_.name()) + " tool request failed: " + s.message());
      return Result<ChatResult>::fail(s.error());
    }
    if (!resp.isSuccess()) {
      if (logger_)
        logger_->warn(std::string(provider_.name()) + " http " + itos(resp.status) + ": " +
                      extractErrorMessage(resp.body));
      return Result<ChatResult>::fail(mapStatus(resp.status));
    }

    AgentTurn turn;
    Status ps = provider_.parseToolResponse(resp, turn);
    if (!ps) return Result<ChatResult>::fail(ps.error());
    agg.inputTokens += turn.inputTokens;
    agg.outputTokens += turn.outputTokens;
    agg.finishReason = turn.finishReason;

    if (!turn.wantsTools()) {
      agg.text = turn.text;  // model produced a final answer
      return Result<ChatResult>::ok(agg);
    }

    // Record the assistant's tool-call turn, then run each tool and append its
    // result for the next iteration.
    Message assistantTurn(Role::Assistant, turn.text);
    assistantTurn.toolCalls = turn.toolCalls;
    messages.push_back(std::move(assistantTurn));

    for (const auto& call : turn.toolCalls) {
      Result<ToolResult> dr = tools.dispatch(call.name, call.argumentsJson);
      ToolResult tr = dr.isOk()
                          ? dr.value()
                          : ToolResult::error(std::string("tool '") + call.name +
                                              "' failed: " + errorString(dr.error()));
      if (logger_)
        logger_->info(std::string("tool ") + call.name + (tr.isError ? " -> error" : " -> ok"));
      messages.push_back(Message::toolResult(call, tr));
    }
  }

  if (logger_) logger_->warn("agent loop reached max iterations");
  return Result<ChatResult>::fail(Error::ToolIterationLimit);
}

Result<ChatResult> LLMClient::run(const std::string& prompt, const ToolRegistry& tools) {
  MessageList m{Message::user(prompt)};
  return doRun(std::move(m), options_, tools);
}

Result<ChatResult> LLMClient::run(const MessageList& messages, const ToolRegistry& tools) {
  return doRun(messages, options_, tools);
}

Result<ChatResult> LLMClient::run(Conversation& convo, const ToolRegistry& tools) {
  ChatOptions o = options_;
  if (!convo.system().empty()) o.system = convo.system();
  Result<ChatResult> r = doRun(convo.messages(), o, tools);
  if (r.isOk()) convo.addAssistant(r.value().text);
  return r;
}

}  // namespace edge
