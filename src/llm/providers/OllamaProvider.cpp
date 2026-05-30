#include "OllamaProvider.h"

#include <ArduinoJson.h>

namespace edge {

namespace {
const char* ollamaRole(Role r) {
  switch (r) {
    case Role::System: return "system";
    case Role::User: return "user";
    case Role::Assistant: return "assistant";
    case Role::Tool: return "tool";
  }
  return "user";
}
}  // namespace

Status OllamaProvider::buildChatRequest(const MessageList& messages, const ChatOptions& options,
                                        bool stream, HttpRequest& out) {
  JsonDocument doc;
  doc["model"] = options.model.empty() ? defaultModel_ : options.model;
  doc["stream"] = stream;

  JsonArray arr = doc["messages"].to<JsonArray>();
  if (!options.system.empty()) {
    JsonObject sys = arr.add<JsonObject>();
    sys["role"] = "system";
    sys["content"] = options.system;
  }
  for (const auto& m : messages) {
    JsonObject o = arr.add<JsonObject>();
    o["role"] = ollamaRole(m.role);
    o["content"] = m.content;
  }

  JsonObject opts = doc["options"].to<JsonObject>();
  opts["num_predict"] = options.maxTokens;
  if (options.hasTemperature()) opts["temperature"] = options.temperature;

  out.method = "POST";
  out.host = host_;
  out.port = port_;
  out.path = "/api/chat";
  out.setHeader("Content-Type", "application/json");
  out.body.clear();
  serializeJson(doc, out.body);
  return Status::ok();
}

Status OllamaProvider::parseChatResponse(const HttpResponse& response, ChatResult& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response.body);
  if (err) return Status::fail(Error::JsonParseError);

  const char* content = doc["message"]["content"];
  if (!content) return Status::fail(Error::ProviderError);
  out.text = content;
  const char* reason = doc["done_reason"];
  if (reason) out.finishReason = reason;
  out.inputTokens = doc["prompt_eval_count"] | 0;
  out.outputTokens = doc["eval_count"] | 0;
  return Status::ok();
}

Status OllamaProvider::parseStreamEvent(const std::string& payload, StreamDelta& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return Status::fail(Error::JsonParseError);

  const char* delta = doc["message"]["content"];
  if (delta) out.textDelta = delta;
  if (doc["done"].as<bool>()) {
    out.done = true;
    const char* reason = doc["done_reason"];
    if (reason) out.finishReason = reason;
    out.inputTokens = doc["prompt_eval_count"] | 0;
    out.outputTokens = doc["eval_count"] | 0;
  }
  return Status::ok();
}

}  // namespace edge
