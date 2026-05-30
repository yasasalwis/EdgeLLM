#include "OpenAIChatProvider.h"

#include <ArduinoJson.h>

namespace edge {

namespace {
const char* openAiRole(Role r) {
  switch (r) {
    case Role::System: return "system";
    case Role::User: return "user";
    case Role::Assistant: return "assistant";
    case Role::Tool: return "tool";
  }
  return "user";
}
}  // namespace

OpenAIChatProvider::OpenAIChatProvider(std::string apiKey, std::string defaultModel,
                                       Endpoint endpoint, const char* providerName)
    : apiKey_(std::move(apiKey)),
      defaultModel_(std::move(defaultModel)),
      endpoint_(std::move(endpoint)),
      name_(providerName) {}

Status OpenAIChatProvider::buildChatRequest(const MessageList& messages, const ChatOptions& options,
                                            bool stream, HttpRequest& out) {
  JsonDocument doc;
  doc["model"] = options.model.empty() ? defaultModel_ : options.model;
  doc["max_tokens"] = options.maxTokens;
  if (options.hasTemperature()) doc["temperature"] = options.temperature;
  if (stream) {
    doc["stream"] = true;
    doc["stream_options"]["include_usage"] = true;  // get usage in the final chunk
  }

  JsonArray arr = doc["messages"].to<JsonArray>();
  if (!options.system.empty()) {
    JsonObject sys = arr.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = options.system;
  }
  for (const auto& m : messages) {
    JsonObject o = arr.add<JsonObject>();
    o["role"] = openAiRole(m.role);
    o["content"] = m.content;
  }

  out.method = "POST";
  out.host = endpoint_.host;
  out.port = endpoint_.port;
  out.path = endpoint_.path;
  out.setHeader("Content-Type", "application/json");
  if (!apiKey_.empty()) out.setHeader(endpoint_.authHeader, endpoint_.authPrefix + apiKey_);
  out.body.clear();
  serializeJson(doc, out.body);
  return Status::ok();
}

Status OpenAIChatProvider::parseChatResponse(const HttpResponse& response, ChatResult& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response.body);
  if (err) return Status::fail(Error::JsonParseError);

  JsonObjectConst choice0 = doc["choices"][0];
  if (choice0.isNull()) return Status::fail(Error::ProviderError);
  const char* content = choice0["message"]["content"];
  out.text = content ? content : "";
  const char* finish = choice0["finish_reason"];
  if (finish) out.finishReason = finish;

  out.inputTokens = doc["usage"]["prompt_tokens"] | 0;
  out.outputTokens = doc["usage"]["completion_tokens"] | 0;
  return Status::ok();
}

Status OpenAIChatProvider::parseStreamEvent(const std::string& payload, StreamDelta& out) {
  // OpenAI marks end-of-stream with a literal [DONE] sentinel.
  if (payload == "[DONE]") {
    out.done = true;
    return Status::ok();
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return Status::fail(Error::JsonParseError);

  JsonObjectConst choice0 = doc["choices"][0];
  if (!choice0.isNull()) {
    const char* delta = choice0["delta"]["content"];
    if (delta) out.textDelta = delta;
    const char* finish = choice0["finish_reason"];
    if (finish) out.finishReason = finish;
  }
  // Final usage chunk (stream_options.include_usage) carries empty choices.
  if (!doc["usage"].isNull()) {
    out.inputTokens = doc["usage"]["prompt_tokens"] | 0;
    out.outputTokens = doc["usage"]["completion_tokens"] | 0;
  }
  return Status::ok();
}

Status OpenAIChatProvider::buildToolRequest(const MessageList& messages, const ChatOptions& options,
                                            const ToolRegistry& tools, HttpRequest& out) {
  JsonDocument doc;
  doc["model"] = options.model.empty() ? defaultModel_ : options.model;
  doc["max_tokens"] = options.maxTokens;
  if (options.hasTemperature()) doc["temperature"] = options.temperature;

  JsonArray arr = doc["messages"].to<JsonArray>();
  if (!options.system.empty()) {
    JsonObject s = arr.add<JsonObject>();
    s["role"] = "system";
    s["content"] = options.system;
  }
  for (const auto& m : messages) {
    if (m.role == Role::System) {
      JsonObject o = arr.add<JsonObject>();
      o["role"] = "system";
      o["content"] = m.content;
    } else if (m.role == Role::Tool) {
      JsonObject o = arr.add<JsonObject>();
      o["role"] = "tool";
      o["tool_call_id"] = m.toolCallId;
      o["content"] = m.content;
    } else if (m.role == Role::Assistant && !m.toolCalls.empty()) {
      JsonObject o = arr.add<JsonObject>();
      o["role"] = "assistant";
      if (!m.content.empty())
        o["content"] = m.content;
      else
        o["content"] = static_cast<const char*>(nullptr);  // OpenAI wants null here
      JsonArray tc = o["tool_calls"].to<JsonArray>();
      for (const auto& call : m.toolCalls) {
        JsonObject c = tc.add<JsonObject>();
        c["id"] = call.id;
        c["type"] = "function";
        JsonObject fn = c["function"].to<JsonObject>();
        fn["name"] = call.name;
        fn["arguments"] = call.argumentsJson.empty() ? "{}" : call.argumentsJson;
      }
    } else {
      JsonObject o = arr.add<JsonObject>();
      o["role"] = (m.role == Role::Assistant) ? "assistant" : "user";
      o["content"] = m.content;
    }
  }

  JsonArray toolsArr = doc["tools"].to<JsonArray>();
  for (const auto& t : tools.tools()) {
    JsonObject to = toolsArr.add<JsonObject>();
    to["type"] = "function";
    JsonObject fn = to["function"].to<JsonObject>();
    fn["name"] = t.name;
    if (!t.description.empty()) fn["description"] = t.description;
    JsonObject params = fn["parameters"].to<JsonObject>();
    writeToolSchema(t, params);
  }

  out.method = "POST";
  out.host = endpoint_.host;
  out.port = endpoint_.port;
  out.path = endpoint_.path;
  out.setHeader("Content-Type", "application/json");
  if (!apiKey_.empty()) out.setHeader(endpoint_.authHeader, endpoint_.authPrefix + apiKey_);
  out.body.clear();
  serializeJson(doc, out.body);
  return Status::ok();
}

Status OpenAIChatProvider::parseToolResponse(const HttpResponse& response, AgentTurn& out) {
  JsonDocument doc;
  if (deserializeJson(doc, response.body)) return Status::fail(Error::JsonParseError);

  JsonObjectConst choice0 = doc["choices"][0];
  if (choice0.isNull()) return Status::fail(Error::ProviderError);
  JsonObjectConst msg = choice0["message"];
  const char* content = msg["content"];
  if (content) out.text = content;

  JsonArrayConst calls = msg["tool_calls"];
  if (!calls.isNull()) {
    for (JsonObjectConst c : calls) {
      ToolCall tc;
      const char* id = c["id"];
      if (id) tc.id = id;
      const char* nm = c["function"]["name"];
      if (nm) tc.name = nm;
      const char* args = c["function"]["arguments"];
      if (args) tc.argumentsJson = args;
      out.toolCalls.push_back(std::move(tc));
    }
  }
  const char* finish = choice0["finish_reason"];
  if (finish) out.finishReason = finish;
  out.inputTokens = doc["usage"]["prompt_tokens"] | 0;
  out.outputTokens = doc["usage"]["completion_tokens"] | 0;
  return Status::ok();
}

}  // namespace edge
