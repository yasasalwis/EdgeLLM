// Tests for each provider's STRUCTURED-OUTPUT request building and response
// parsing (the only response mode). Request bodies are deserialized with
// ArduinoJson and asserted structurally.
#include <ArduinoJson.h>

#include "../../src/llm/ResponseSchema.h"
#include "../../src/llm/providers/AnthropicProvider.h"
#include "../../src/llm/providers/GeminiProvider.h"
#include "../../src/llm/providers/OllamaProvider.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
ResponseSchema sampleSchema() {
  ResponseSchema s("result");
  s.field("answer", ParamType::String, "the answer");
  s.field("score", ParamType::Integer, "0-100", false);
  return s;
}
JsonDocument parse(const std::string& body) {
  JsonDocument d;
  deserializeJson(d, body);
  return d;
}
std::string jstr(JsonVariantConst v) {
  const char* p = v.as<const char*>();
  return p ? std::string(p) : std::string();
}
MessageList userMsg() { return MessageList{Message::user("hi")}; }
}  // namespace

// ----------------------------- OpenAI -----------------------------

TEST(openai_structured_request_uses_json_schema) {
  OpenAIProvider p("k", "gpt-x");
  ChatOptions o;
  o.system = "be precise";
  HttpRequest req;
  CHECK(p.buildStructuredRequest(userMsg(), o, sampleSchema(), req).isOk());
  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(jstr(d["response_format"]["type"]), "json_schema");
  CHECK_STR_EQ(jstr(d["response_format"]["json_schema"]["name"]), "result");
  CHECK_STR_EQ(jstr(d["response_format"]["json_schema"]["schema"]["type"]), "object");
  CHECK_STR_EQ(jstr(d["messages"][0]["role"]), "system");
  CHECK_STR_EQ(jstr(d["messages"][1]["content"]), "hi");
}

TEST(openai_structured_response_extracts_json) {
  OpenAIProvider p("k");
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"choices":[{"message":{"content":"{\"answer\":\"42\",\"score\":9}"},"finish_reason":"stop"}],"usage":{"prompt_tokens":3,"completion_tokens":4}})";
  std::string json;
  ChatResult meta;
  CHECK(p.parseStructuredResponse(resp, json, meta).isOk());
  CHECK_STR_EQ(json, "{\"answer\":\"42\",\"score\":9}");
  CHECK_EQ(meta.outputTokens, static_cast<uint32_t>(4));
}

// ----------------------------- Anthropic -----------------------------

TEST(anthropic_structured_request_forces_tool) {
  AnthropicProvider p("k", "claude-x");
  HttpRequest req;
  CHECK(p.buildStructuredRequest(userMsg(), ChatOptions{}, sampleSchema(), req).isOk());
  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(jstr(d["tools"][0]["name"]), "result");
  CHECK_STR_EQ(jstr(d["tools"][0]["input_schema"]["type"]), "object");
  CHECK_STR_EQ(jstr(d["tool_choice"]["type"]), "tool");
  CHECK_STR_EQ(jstr(d["tool_choice"]["name"]), "result");
}

TEST(anthropic_structured_response_reads_tool_use_input) {
  AnthropicProvider p("k");
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"content":[{"type":"tool_use","id":"t1","name":"result","input":{"answer":"42","score":9}}],"stop_reason":"tool_use","usage":{"input_tokens":5,"output_tokens":6}})";
  std::string json;
  ChatResult meta;
  CHECK(p.parseStructuredResponse(resp, json, meta).isOk());
  CHECK(json.find("\"answer\":\"42\"") != std::string::npos);
  CHECK_EQ(meta.outputTokens, static_cast<uint32_t>(6));
}

// ----------------------------- Gemini -----------------------------

TEST(gemini_structured_request_sets_response_schema) {
  GeminiProvider p("KEY", "gemini-x");
  HttpRequest req;
  CHECK(p.buildStructuredRequest(userMsg(), ChatOptions{}, sampleSchema(), req).isOk());
  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(jstr(d["generationConfig"]["responseMimeType"]), "application/json");
  CHECK_STR_EQ(jstr(d["generationConfig"]["responseSchema"]["type"]), "object");
  // Gemini's dialect rejects additionalProperties — it must be absent.
  CHECK(d["generationConfig"]["responseSchema"]["additionalProperties"].isNull());
}

TEST(gemini_structured_response_extracts_json_text) {
  GeminiProvider p("KEY");
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"candidates":[{"content":{"parts":[{"text":"{\"answer\":\"42\"}"}]},"finishReason":"STOP"}],"usageMetadata":{"promptTokenCount":2,"candidatesTokenCount":1}})";
  std::string json;
  ChatResult meta;
  CHECK(p.parseStructuredResponse(resp, json, meta).isOk());
  CHECK_STR_EQ(json, "{\"answer\":\"42\"}");
}

// ----------------------------- Ollama -----------------------------

TEST(ollama_structured_request_sets_format) {
  OllamaProvider p("127.0.0.1", 11434, "llama3.2", false);
  HttpRequest req;
  CHECK(p.buildStructuredRequest(userMsg(), ChatOptions{}, sampleSchema(), req).isOk());
  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(jstr(d["format"]["type"]), "object");
  CHECK(!d["stream"].as<bool>());
}

TEST(ollama_structured_response_extracts_content) {
  OllamaProvider p;
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"message":{"content":"{\"answer\":\"42\"}"},"done":true,"prompt_eval_count":7,"eval_count":3})";
  std::string json;
  ChatResult meta;
  CHECK(p.parseStructuredResponse(resp, json, meta).isOk());
  CHECK_STR_EQ(json, "{\"answer\":\"42\"}");
  CHECK_EQ(meta.outputTokens, static_cast<uint32_t>(3));
}
