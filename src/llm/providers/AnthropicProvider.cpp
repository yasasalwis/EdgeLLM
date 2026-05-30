#include "AnthropicProvider.h"

#include <ArduinoJson.h>

namespace edge {

Status AnthropicProvider::buildChatRequest(const MessageList& messages, const ChatOptions& options,
                                           bool stream, HttpRequest& out) {
  JsonDocument doc;
  doc["model"] = options.model.empty() ? defaultModel_ : options.model;
  doc["max_tokens"] = options.maxTokens;
  if (options.hasTemperature()) doc["temperature"] = options.temperature;
  if (stream) doc["stream"] = true;

  // System prompt is a top-level field. Fold in both ChatOptions.system and any
  // inline System-role messages.
  std::string system = options.system;
  for (const auto& m : messages) {
    if (m.role == Role::System) {
      if (!system.empty()) system += "\n\n";
      system += m.content;
    }
  }
  if (!system.empty()) doc["system"] = system;

  JsonArray arr = doc["messages"].to<JsonArray>();
  for (const auto& m : messages) {
    if (m.role == Role::System) continue;  // handled above
    JsonObject o = arr.add<JsonObject>();
    o["role"] = (m.role == Role::Assistant) ? "assistant" : "user";
    o["content"] = m.content;
  }

  out.method = "POST";
  out.host = "api.anthropic.com";
  out.port = 443;
  out.path = "/v1/messages";
  out.setHeader("Content-Type", "application/json");
  out.setHeader("x-api-key", apiKey_);
  out.setHeader("anthropic-version", apiVersion_);
  out.body.clear();
  serializeJson(doc, out.body);
  return Status::ok();
}

Status AnthropicProvider::parseChatResponse(const HttpResponse& response, ChatResult& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response.body);
  if (err) return Status::fail(Error::JsonParseError);

  JsonArrayConst content = doc["content"];
  if (content.isNull()) return Status::fail(Error::ProviderError);
  for (JsonObjectConst block : content) {
    const char* type = block["type"];
    if (type && std::string(type) == "text") {
      const char* text = block["text"];
      if (text) out.text += text;
    }
  }
  const char* stop = doc["stop_reason"];
  if (stop) out.finishReason = stop;
  out.inputTokens = doc["usage"]["input_tokens"] | 0;
  out.outputTokens = doc["usage"]["output_tokens"] | 0;
  return Status::ok();
}

Status AnthropicProvider::parseStreamEvent(const std::string& payload, StreamDelta& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return Status::fail(Error::JsonParseError);

  const char* type = doc["type"];
  if (!type) return Status::ok();  // keep-alive / unknown; nothing to emit
  const std::string t = type;

  if (t == "content_block_delta") {
    const char* text = doc["delta"]["text"];
    if (text) out.textDelta = text;
  } else if (t == "message_start") {
    out.inputTokens = doc["message"]["usage"]["input_tokens"] | 0;
  } else if (t == "message_delta") {
    const char* stop = doc["delta"]["stop_reason"];
    if (stop) out.finishReason = stop;
    out.outputTokens = doc["usage"]["output_tokens"] | 0;
  } else if (t == "message_stop") {
    out.done = true;
  }
  return Status::ok();
}

Status AnthropicProvider::buildToolRequest(const MessageList& messages, const ChatOptions& options,
                                           const ToolRegistry& tools, HttpRequest& out) {
  JsonDocument doc;
  doc["model"] = options.model.empty() ? defaultModel_ : options.model;
  doc["max_tokens"] = options.maxTokens;
  if (options.hasTemperature()) doc["temperature"] = options.temperature;

  std::string system = options.system;
  for (const auto& m : messages) {
    if (m.role == Role::System) {
      if (!system.empty()) system += "\n\n";
      system += m.content;
    }
  }
  if (!system.empty()) doc["system"] = system;

  JsonArray arr = doc["messages"].to<JsonArray>();
  size_t i = 0;
  while (i < messages.size()) {
    const Message& m = messages[i];
    if (m.role == Role::System) {
      ++i;
      continue;
    }
    if (m.role == Role::Tool) {
      // Anthropic requires all tool_result blocks in a single user message.
      JsonObject o = arr.add<JsonObject>();
      o["role"] = "user";
      JsonArray content = o["content"].to<JsonArray>();
      while (i < messages.size() && messages[i].role == Role::Tool) {
        const Message& tm = messages[i];
        JsonObject block = content.add<JsonObject>();
        block["type"] = "tool_result";
        block["tool_use_id"] = tm.toolCallId;
        block["content"] = tm.content;
        if (tm.isToolError) block["is_error"] = true;
        ++i;
      }
      continue;
    }
    if (m.role == Role::Assistant && !m.toolCalls.empty()) {
      JsonObject o = arr.add<JsonObject>();
      o["role"] = "assistant";
      JsonArray content = o["content"].to<JsonArray>();
      if (!m.content.empty()) {
        JsonObject text = content.add<JsonObject>();
        text["type"] = "text";
        text["text"] = m.content;
      }
      for (const auto& call : m.toolCalls) {
        JsonObject b = content.add<JsonObject>();
        b["type"] = "tool_use";
        b["id"] = call.id;
        b["name"] = call.name;
        if (call.argumentsJson.empty()) {
          b["input"].to<JsonObject>();  // empty object
        } else {
          JsonDocument argDoc;
          if (deserializeJson(argDoc, call.argumentsJson))
            b["input"].to<JsonObject>();
          else
            b["input"] = argDoc;
        }
      }
      ++i;
      continue;
    }
    JsonObject o = arr.add<JsonObject>();
    o["role"] = (m.role == Role::Assistant) ? "assistant" : "user";
    o["content"] = m.content;
    ++i;
  }

  JsonArray toolsArr = doc["tools"].to<JsonArray>();
  for (const auto& t : tools.tools()) {
    JsonObject to = toolsArr.add<JsonObject>();
    to["name"] = t.name;
    if (!t.description.empty()) to["description"] = t.description;
    JsonObject schema = to["input_schema"].to<JsonObject>();
    writeToolSchema(t, schema);
  }

  out.method = "POST";
  out.host = "api.anthropic.com";
  out.port = 443;
  out.path = "/v1/messages";
  out.setHeader("Content-Type", "application/json");
  out.setHeader("x-api-key", apiKey_);
  out.setHeader("anthropic-version", apiVersion_);
  out.body.clear();
  serializeJson(doc, out.body);
  return Status::ok();
}

Status AnthropicProvider::parseToolResponse(const HttpResponse& response, AgentTurn& out) {
  JsonDocument doc;
  if (deserializeJson(doc, response.body)) return Status::fail(Error::JsonParseError);

  JsonArrayConst content = doc["content"];
  if (content.isNull()) return Status::fail(Error::ProviderError);
  for (JsonObjectConst block : content) {
    const char* type = block["type"];
    if (!type) continue;
    const std::string t = type;
    if (t == "text") {
      const char* tx = block["text"];
      if (tx) out.text += tx;
    } else if (t == "tool_use") {
      ToolCall tc;
      const char* id = block["id"];
      if (id) tc.id = id;
      const char* nm = block["name"];
      if (nm) tc.name = nm;
      JsonObjectConst input = block["input"];
      if (!input.isNull()) serializeJson(input, tc.argumentsJson);
      out.toolCalls.push_back(std::move(tc));
    }
  }
  const char* stop = doc["stop_reason"];
  if (stop) out.finishReason = stop;
  out.inputTokens = doc["usage"]["input_tokens"] | 0;
  out.outputTokens = doc["usage"]["output_tokens"] | 0;
  return Status::ok();
}

}  // namespace edge
