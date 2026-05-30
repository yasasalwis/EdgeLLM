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

Status OllamaProvider::buildToolRequest(const MessageList& messages, const ChatOptions& options,
                                        const ToolRegistry& tools, HttpRequest& out) {
  JsonDocument doc;
  doc["model"] = options.model.empty() ? defaultModel_ : options.model;
  doc["stream"] = false;  // tool calls are handled non-streaming

  JsonArray arr = doc["messages"].to<JsonArray>();
  if (!options.system.empty()) {
    JsonObject s = arr.add<JsonObject>();
    s["role"] = "system";
    s["content"] = options.system;
  }
  for (const auto& m : messages) {
    if (m.role == Role::Tool) {
      JsonObject o = arr.add<JsonObject>();
      o["role"] = "tool";
      o["content"] = m.content;
      if (!m.toolName.empty()) o["tool_name"] = m.toolName;  // newer Ollama
    } else if (m.role == Role::Assistant && !m.toolCalls.empty()) {
      JsonObject o = arr.add<JsonObject>();
      o["role"] = "assistant";
      o["content"] = m.content;
      JsonArray tc = o["tool_calls"].to<JsonArray>();
      for (const auto& call : m.toolCalls) {
        JsonObject fn = tc.add<JsonObject>()["function"].to<JsonObject>();
        fn["name"] = call.name;
        // Ollama expects arguments as a JSON object, not a string.
        if (call.argumentsJson.empty()) {
          fn["arguments"].to<JsonObject>();
        } else {
          JsonDocument argDoc;
          if (deserializeJson(argDoc, call.argumentsJson))
            fn["arguments"].to<JsonObject>();
          else
            fn["arguments"] = argDoc;
        }
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
  out.host = host_;
  out.port = port_;
  out.path = "/api/chat";
  out.setHeader("Content-Type", "application/json");
  out.body.clear();
  serializeJson(doc, out.body);
  return Status::ok();
}

Status OllamaProvider::parseToolResponse(const HttpResponse& response, AgentTurn& out) {
  JsonDocument doc;
  if (deserializeJson(doc, response.body)) return Status::fail(Error::JsonParseError);

  JsonObjectConst msg = doc["message"];
  const char* content = msg["content"];
  if (content) out.text = content;

  JsonArrayConst calls = msg["tool_calls"];
  if (!calls.isNull()) {
    for (JsonObjectConst c : calls) {
      ToolCall tc;
      const char* nm = c["function"]["name"];
      if (nm) {
        tc.name = nm;
        tc.id = nm;  // Ollama has no call id; match by name
      }
      JsonObjectConst args = c["function"]["arguments"];
      if (!args.isNull()) serializeJson(args, tc.argumentsJson);
      out.toolCalls.push_back(std::move(tc));
    }
  }
  const char* reason = doc["done_reason"];
  if (reason) out.finishReason = reason;
  out.inputTokens = doc["prompt_eval_count"] | 0;
  out.outputTokens = doc["eval_count"] | 0;
  return Status::ok();
}

}  // namespace edge
