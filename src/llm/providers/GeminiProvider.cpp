#include "GeminiProvider.h"

#include <ArduinoJson.h>

namespace edge {

namespace {
// Writes a message's parts: text plus, when present, an inline_data image part.
void writeGeminiParts(JsonObject o, const Message& m) {
  JsonArray parts = o["parts"].to<JsonArray>();
  if (!m.content.empty() || !m.hasImage()) {
    parts.add<JsonObject>()["text"] = m.content;
  }
  if (m.hasImage()) {
    JsonObject inline_ = parts.add<JsonObject>()["inline_data"].to<JsonObject>();
    inline_["mime_type"] = m.imageMime.empty() ? "image/jpeg" : m.imageMime;
    inline_["data"] = m.imageBase64;
  }
}
}  // namespace

Status GeminiProvider::buildStructuredRequest(const MessageList& messages,
                                              const ChatOptions& options,
                                              const ResponseSchema& schema, HttpRequest& out) {
  const std::string model = options.model.empty() ? defaultModel_ : options.model;
  JsonDocument doc;

  std::string system = options.system;
  for (const auto& m : messages) {
    if (m.role == Role::System) {
      if (!system.empty()) system += "\n\n";
      system += m.content;
    }
  }
  if (!system.empty()) doc["systemInstruction"]["parts"][0]["text"] = system;

  JsonArray contents = doc["contents"].to<JsonArray>();
  for (const auto& m : messages) {
    if (m.role == Role::System) continue;
    JsonObject o = contents.add<JsonObject>();
    o["role"] = (m.role == Role::Assistant) ? "model" : "user";
    writeGeminiParts(o, m);
  }

  JsonObject gen = doc["generationConfig"].to<JsonObject>();
  gen["maxOutputTokens"] = options.maxTokens;
  if (options.hasTemperature()) gen["temperature"] = options.temperature;
  if (options.hasTopP()) gen["topP"] = options.topP;
  if (!options.stopSequences.empty()) {
    JsonArray stops = gen["stopSequences"].to<JsonArray>();
    for (const auto& seq : options.stopSequences)
      stops.add(seq);
  }
  // Native structured output. Gemini's responseSchema is an OpenAPI subset that
  // rejects additionalProperties, so omit it.
  gen["responseMimeType"] = "application/json";
  JsonObject rs = gen["responseSchema"].to<JsonObject>();
  schema.writeSchema(rs, /*additionalPropertiesFalse=*/false);

  out.method = "POST";
  out.host = "generativelanguage.googleapis.com";
  out.port = 443;
  out.path = "/v1beta/models/" + model + ":generateContent?key=" + apiKey_;
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

Status GeminiProvider::parseStructuredResponse(const HttpResponse& response, std::string& jsonOut,
                                               ChatResult& meta) {
  JsonDocument doc;
  if (deserializeJson(doc, response.body)) return Status::fail(Error::JsonParseError);

  JsonObjectConst cand0 = doc["candidates"][0];
  if (cand0.isNull()) return Status::fail(Error::ProviderError);
  // With responseMimeType application/json, the text parts ARE the JSON output.
  appendParts(cand0, jsonOut);
  if (jsonOut.empty()) return Status::fail(Error::ProviderError);
  const char* finish = cand0["finishReason"];
  if (finish) meta.finishReason = finish;
  meta.inputTokens = doc["usageMetadata"]["promptTokenCount"] | 0;
  meta.outputTokens = doc["usageMetadata"]["candidatesTokenCount"] | 0;
  return Status::ok();
}

Status GeminiProvider::buildToolRequest(const MessageList& messages, const ChatOptions& options,
                                        const ToolRegistry& tools, HttpRequest& out) {
  const std::string model = options.model.empty() ? defaultModel_ : options.model;
  JsonDocument doc;

  std::string system = options.system;
  for (const auto& m : messages) {
    if (m.role == Role::System) {
      if (!system.empty()) system += "\n\n";
      system += m.content;
    }
  }
  if (!system.empty()) doc["systemInstruction"]["parts"][0]["text"] = system;

  JsonArray contents = doc["contents"].to<JsonArray>();
  size_t i = 0;
  while (i < messages.size()) {
    const Message& m = messages[i];
    if (m.role == Role::System) {
      ++i;
      continue;
    }
    if (m.role == Role::Tool) {
      // Function responses go in a single user content (Gemini has no tool role).
      JsonObject o = contents.add<JsonObject>();
      o["role"] = "user";
      JsonArray parts = o["parts"].to<JsonArray>();
      while (i < messages.size() && messages[i].role == Role::Tool) {
        const Message& tm = messages[i];
        JsonObject fr = parts.add<JsonObject>()["functionResponse"].to<JsonObject>();
        fr["name"] = tm.toolName;
        fr["response"]["result"] = tm.content;
        ++i;
      }
      continue;
    }
    if (m.role == Role::Assistant && !m.toolCalls.empty()) {
      JsonObject o = contents.add<JsonObject>();
      o["role"] = "model";
      JsonArray parts = o["parts"].to<JsonArray>();
      if (!m.content.empty()) parts.add<JsonObject>()["text"] = m.content;
      for (const auto& call : m.toolCalls) {
        JsonObject fc = parts.add<JsonObject>()["functionCall"].to<JsonObject>();
        fc["name"] = call.name;
        if (call.argumentsJson.empty()) {
          fc["args"].to<JsonObject>();
        } else {
          JsonDocument argDoc;
          if (deserializeJson(argDoc, call.argumentsJson))
            fc["args"].to<JsonObject>();
          else
            fc["args"] = argDoc;
        }
      }
      ++i;
      continue;
    }
    JsonObject o = contents.add<JsonObject>();
    o["role"] = (m.role == Role::Assistant) ? "model" : "user";
    writeGeminiParts(o, m);
    ++i;
  }

  JsonArray decls = doc["tools"][0]["functionDeclarations"].to<JsonArray>();
  for (const auto& t : tools.tools()) {
    JsonObject d = decls.add<JsonObject>();
    d["name"] = t.name;
    if (!t.description.empty()) d["description"] = t.description;
    JsonObject params = d["parameters"].to<JsonObject>();
    writeToolSchema(t, params);
  }

  JsonObject gen = doc["generationConfig"].to<JsonObject>();
  gen["maxOutputTokens"] = options.maxTokens;
  if (options.hasTemperature()) gen["temperature"] = options.temperature;
  if (options.hasTopP()) gen["topP"] = options.topP;
  if (!options.stopSequences.empty()) {
    JsonArray stops = gen["stopSequences"].to<JsonArray>();
    for (const auto& seq : options.stopSequences)
      stops.add(seq);
  }

  out.method = "POST";
  out.host = "generativelanguage.googleapis.com";
  out.port = 443;
  out.path = "/v1beta/models/" + model + ":generateContent?key=" + apiKey_;
  out.setHeader("Content-Type", "application/json");
  out.body.clear();
  serializeJson(doc, out.body);
  return Status::ok();
}

Status GeminiProvider::parseToolResponse(const HttpResponse& response, AgentTurn& out) {
  JsonDocument doc;
  if (deserializeJson(doc, response.body)) return Status::fail(Error::JsonParseError);

  JsonObjectConst cand0 = doc["candidates"][0];
  if (cand0.isNull()) return Status::fail(Error::ProviderError);
  JsonArrayConst parts = cand0["content"]["parts"];
  if (!parts.isNull()) {
    for (JsonObjectConst part : parts) {
      const char* text = part["text"];
      if (text) out.text += text;
      JsonObjectConst fc = part["functionCall"];
      if (!fc.isNull()) {
        ToolCall tc;
        const char* nm = fc["name"];
        if (nm) {
          tc.name = nm;
          tc.id = nm;  // Gemini matches function responses by name, not id
        }
        JsonObjectConst args = fc["args"];
        if (!args.isNull()) serializeJson(args, tc.argumentsJson);
        out.toolCalls.push_back(std::move(tc));
      }
    }
  }
  const char* finish = cand0["finishReason"];
  if (finish) out.finishReason = finish;
  out.inputTokens = doc["usageMetadata"]["promptTokenCount"] | 0;
  out.outputTokens = doc["usageMetadata"]["candidatesTokenCount"] | 0;
  return Status::ok();
}

}  // namespace edge
