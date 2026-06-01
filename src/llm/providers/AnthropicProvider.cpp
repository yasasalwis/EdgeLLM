#include "AnthropicProvider.h"

#include <ArduinoJson.h>

namespace edge {

Status AnthropicProvider::buildStructuredRequest(const MessageList& messages,
                                                 const ChatOptions& options,
                                                 const ResponseSchema& schema, HttpRequest& out) {
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
  for (const auto& m : messages) {
    if (m.role == Role::System) continue;
    JsonObject o = arr.add<JsonObject>();
    o["role"] = (m.role == Role::Assistant) ? "assistant" : "user";
    o["content"] = m.content;
  }

  // Anthropic has no response_format; structured output is achieved by exposing
  // a single tool whose input_schema is the response schema and forcing its use.
  JsonArray toolsArr = doc["tools"].to<JsonArray>();
  JsonObject tool = toolsArr.add<JsonObject>();
  tool["name"] = schema.name();
  tool["description"] = "Return the result in the required structure.";
  JsonObject inputSchema = tool["input_schema"].to<JsonObject>();
  schema.writeSchema(inputSchema);
  JsonObject choice = doc["tool_choice"].to<JsonObject>();
  choice["type"] = "tool";
  choice["name"] = schema.name();

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

Status AnthropicProvider::parseStructuredResponse(const HttpResponse& response,
                                                  std::string& jsonOut, ChatResult& meta) {
  JsonDocument doc;
  if (deserializeJson(doc, response.body)) return Status::fail(Error::JsonParseError);

  JsonArrayConst content = doc["content"];
  if (content.isNull()) return Status::fail(Error::ProviderError);
  bool found = false;
  for (JsonObjectConst block : content) {
    const char* type = block["type"];
    if (type && std::string(type) == "tool_use") {
      JsonObjectConst input = block["input"];
      if (!input.isNull()) {
        serializeJson(input, jsonOut);
        found = true;
        break;
      }
    }
  }
  if (!found) return Status::fail(Error::ProviderError);
  const char* stop = doc["stop_reason"];
  if (stop) meta.finishReason = stop;
  meta.inputTokens = doc["usage"]["input_tokens"] | 0;
  meta.outputTokens = doc["usage"]["output_tokens"] | 0;
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
