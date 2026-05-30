#include "GeminiProvider.h"

#include <ArduinoJson.h>

namespace edge {

Status GeminiProvider::buildChatRequest(const MessageList& messages, const ChatOptions& options,
                                        bool stream, HttpRequest& out) {
  const std::string model = options.model.empty() ? defaultModel_ : options.model;

  JsonDocument doc;

  std::string system = options.system;
  for (const auto& m : messages) {
    if (m.role == Role::System) {
      if (!system.empty()) system += "\n\n";
      system += m.content;
    }
  }
  if (!system.empty()) {
    doc["systemInstruction"]["parts"][0]["text"] = system;
  }

  JsonArray contents = doc["contents"].to<JsonArray>();
  for (const auto& m : messages) {
    if (m.role == Role::System) continue;
    JsonObject o = contents.add<JsonObject>();
    o["role"] = (m.role == Role::Assistant) ? "model" : "user";
    o["parts"][0]["text"] = m.content;
  }

  JsonObject gen = doc["generationConfig"].to<JsonObject>();
  gen["maxOutputTokens"] = options.maxTokens;
  if (options.hasTemperature()) gen["temperature"] = options.temperature;

  out.method = "POST";
  out.host = "generativelanguage.googleapis.com";
  out.port = 443;
  const char* verb = stream ? ":streamGenerateContent" : ":generateContent";
  out.path = "/v1beta/models/" + model + verb + (stream ? "?alt=sse&key=" : "?key=") + apiKey_;
  out.setHeader("Content-Type", "application/json");
  out.body.clear();
  serializeJson(doc, out.body);
  return Status::ok();
}

namespace {
// Concatenates all text parts in a candidate's content into `dst`.
void appendParts(JsonObjectConst candidate, std::string& dst) {
  JsonArrayConst parts = candidate["content"]["parts"];
  if (parts.isNull()) return;
  for (JsonObjectConst part : parts) {
    const char* text = part["text"];
    if (text) dst += text;
  }
}
}  // namespace

Status GeminiProvider::parseChatResponse(const HttpResponse& response, ChatResult& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response.body);
  if (err) return Status::fail(Error::JsonParseError);

  JsonObjectConst cand0 = doc["candidates"][0];
  if (cand0.isNull()) return Status::fail(Error::ProviderError);
  appendParts(cand0, out.text);
  const char* finish = cand0["finishReason"];
  if (finish) out.finishReason = finish;
  out.inputTokens = doc["usageMetadata"]["promptTokenCount"] | 0;
  out.outputTokens = doc["usageMetadata"]["candidatesTokenCount"] | 0;
  return Status::ok();
}

Status GeminiProvider::parseStreamEvent(const std::string& payload, StreamDelta& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return Status::fail(Error::JsonParseError);

  JsonObjectConst cand0 = doc["candidates"][0];
  if (!cand0.isNull()) {
    appendParts(cand0, out.textDelta);
    const char* finish = cand0["finishReason"];
    if (finish) {
      out.finishReason = finish;
      out.done = true;  // Gemini sends finishReason on the terminal SSE event
    }
  }
  if (!doc["usageMetadata"].isNull()) {
    out.inputTokens = doc["usageMetadata"]["promptTokenCount"] | 0;
    out.outputTokens = doc["usageMetadata"]["candidatesTokenCount"] | 0;
  }
  return Status::ok();
}

}  // namespace edge
