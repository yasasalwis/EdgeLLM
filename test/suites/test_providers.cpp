// Tests for each Provider's request building, response parsing, and stream
// event parsing. Request bodies are deserialized back with ArduinoJson and
// asserted structurally (robust against key ordering / whitespace).
#include <ArduinoJson.h>

#include "../../src/llm/providers/AnthropicProvider.h"
#include "../../src/llm/providers/GeminiProvider.h"
#include "../../src/llm/providers/OllamaProvider.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
const std::string* findHeader(const HttpRequest& req, const std::string& name) {
  return req.header(name);
}
JsonDocument parse(const std::string& body) {
  JsonDocument doc;
  deserializeJson(doc, body);
  return doc;
}
}  // namespace

// --------------------------- Anthropic ---------------------------

TEST(anthropic_builds_request) {
  AnthropicProvider p("sk-ant-key", "claude-x");
  ChatOptions o;
  o.system = "be brief";
  o.maxTokens = 50;
  o.temperature = 0.5f;
  MessageList m{Message::user("hi")};
  HttpRequest req;
  CHECK(p.buildChatRequest(m, o, false, req).isOk());

  CHECK_STR_EQ(req.host, "api.anthropic.com");
  CHECK_STR_EQ(req.path, "/v1/messages");
  CHECK_STR_EQ(req.method, "POST");
  CHECK(findHeader(req, "x-api-key") != nullptr);
  CHECK_STR_EQ(*findHeader(req, "x-api-key"), "sk-ant-key");
  CHECK(findHeader(req, "anthropic-version") != nullptr);

  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(std::string(d["model"].as<const char*>()), "claude-x");
  CHECK_EQ(d["max_tokens"].as<int>(), 50);
  CHECK_STR_EQ(std::string(d["system"].as<const char*>()), "be brief");
  CHECK(d["temperature"].as<float>() == 0.5f);
  CHECK_STR_EQ(std::string(d["messages"][0]["role"].as<const char*>()), "user");
  CHECK_STR_EQ(std::string(d["messages"][0]["content"].as<const char*>()), "hi");
  CHECK(d["stream"].isNull());  // not streaming
}

TEST(anthropic_stream_flag_set) {
  AnthropicProvider p("k");
  ChatOptions o;
  MessageList m{Message::user("hi")};
  HttpRequest req;
  p.buildChatRequest(m, o, true, req);
  JsonDocument d = parse(req.body);
  CHECK(d["stream"].as<bool>());
}

TEST(anthropic_parses_response) {
  AnthropicProvider p("k");
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"content":[{"type":"text","text":"Hello"}],"stop_reason":"end_turn","usage":{"input_tokens":10,"output_tokens":5}})";
  ChatResult cr;
  CHECK(p.parseChatResponse(resp, cr).isOk());
  CHECK_STR_EQ(cr.text, "Hello");
  CHECK_STR_EQ(cr.finishReason, "end_turn");
  CHECK_EQ(cr.inputTokens, static_cast<uint32_t>(10));
  CHECK_EQ(cr.outputTokens, static_cast<uint32_t>(5));
}

TEST(anthropic_parses_stream_events) {
  AnthropicProvider p("k");
  StreamDelta d1;
  p.parseStreamEvent(R"({"type":"content_block_delta","delta":{"type":"text_delta","text":"He"}})",
                     d1);
  CHECK_STR_EQ(d1.textDelta, "He");
  CHECK(!d1.done);

  StreamDelta d2;
  p.parseStreamEvent(R"({"type":"message_stop"})", d2);
  CHECK(d2.done);
}

// --------------------------- OpenAI ---------------------------

TEST(openai_builds_request_with_bearer_and_system) {
  OpenAIProvider p("sk-key", "gpt-x");
  ChatOptions o;
  o.system = "sys";
  MessageList m{Message::user("hi")};
  HttpRequest req;
  p.buildChatRequest(m, o, false, req);

  CHECK_STR_EQ(req.host, "api.openai.com");
  CHECK_STR_EQ(req.path, "/v1/chat/completions");
  CHECK_STR_EQ(*findHeader(req, "Authorization"), "Bearer sk-key");

  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(std::string(d["messages"][0]["role"].as<const char*>()), "system");
  CHECK_STR_EQ(std::string(d["messages"][0]["content"].as<const char*>()), "sys");
  CHECK_STR_EQ(std::string(d["messages"][1]["role"].as<const char*>()), "user");
  CHECK_STR_EQ(std::string(d["model"].as<const char*>()), "gpt-x");
}

TEST(openai_stream_requests_usage) {
  OpenAIProvider p("k");
  ChatOptions o;
  MessageList m{Message::user("hi")};
  HttpRequest req;
  p.buildChatRequest(m, o, true, req);
  JsonDocument d = parse(req.body);
  CHECK(d["stream"].as<bool>());
  CHECK(d["stream_options"]["include_usage"].as<bool>());
}

TEST(openai_parses_response_and_stream) {
  OpenAIProvider p("k");
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"choices":[{"message":{"content":"Hi there"},"finish_reason":"stop"}],"usage":{"prompt_tokens":3,"completion_tokens":4}})";
  ChatResult cr;
  CHECK(p.parseChatResponse(resp, cr).isOk());
  CHECK_STR_EQ(cr.text, "Hi there");
  CHECK_EQ(cr.outputTokens, static_cast<uint32_t>(4));

  StreamDelta d;
  p.parseStreamEvent(R"({"choices":[{"delta":{"content":"Hi"}}]})", d);
  CHECK_STR_EQ(d.textDelta, "Hi");
  StreamDelta done;
  p.parseStreamEvent("[DONE]", done);
  CHECK(done.done);
}

TEST(openai_compatible_endpoint_is_configurable) {
  OpenAIChatProvider::Endpoint ep;
  ep.host = "localhost";
  ep.port = 8080;
  ep.secure = false;
  OpenAIChatProvider p("k", "m", ep, "groq");
  CHECK_STR_EQ(p.name(), "groq");
  CHECK(!p.secure());
  HttpRequest req;
  ChatOptions o;
  MessageList m{Message::user("hi")};
  p.buildChatRequest(m, o, false, req);
  CHECK_STR_EQ(req.host, "localhost");
  CHECK_EQ(req.port, static_cast<uint16_t>(8080));
}

// --------------------------- Gemini ---------------------------

TEST(gemini_builds_request_with_key_in_path) {
  GeminiProvider p("KEY123", "gemini-x");
  ChatOptions o;
  o.system = "sys";
  MessageList m{Message::user("hi")};
  HttpRequest req;
  p.buildChatRequest(m, o, false, req);
  CHECK_STR_EQ(req.host, "generativelanguage.googleapis.com");
  CHECK_STR_EQ(req.path, "/v1beta/models/gemini-x:generateContent?key=KEY123");

  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(std::string(d["contents"][0]["role"].as<const char*>()), "user");
  CHECK_STR_EQ(std::string(d["contents"][0]["parts"][0]["text"].as<const char*>()), "hi");
  CHECK_STR_EQ(std::string(d["systemInstruction"]["parts"][0]["text"].as<const char*>()), "sys");
}

TEST(gemini_stream_path_uses_sse) {
  GeminiProvider p("K", "gemini-x");
  ChatOptions o;
  MessageList m{Message::user("hi")};
  HttpRequest req;
  p.buildChatRequest(m, o, true, req);
  CHECK(req.path.find(":streamGenerateContent?alt=sse&key=K") != std::string::npos);
}

TEST(gemini_parses_response_and_stream) {
  GeminiProvider p("K");
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"candidates":[{"content":{"parts":[{"text":"Hi"}]},"finishReason":"STOP"}],"usageMetadata":{"promptTokenCount":2,"candidatesTokenCount":1}})";
  ChatResult cr;
  CHECK(p.parseChatResponse(resp, cr).isOk());
  CHECK_STR_EQ(cr.text, "Hi");
  CHECK_EQ(cr.inputTokens, static_cast<uint32_t>(2));

  StreamDelta d;
  p.parseStreamEvent(R"({"candidates":[{"content":{"parts":[{"text":"Hi"}]}}]})", d);
  CHECK_STR_EQ(d.textDelta, "Hi");
  CHECK(!d.done);
  StreamDelta last;
  p.parseStreamEvent(R"({"candidates":[{"content":{"parts":[{"text":"!"}]},"finishReason":"STOP"}]})",
                     last);
  CHECK(last.done);
}

// --------------------------- Ollama ---------------------------

TEST(ollama_builds_local_request) {
  OllamaProvider p("10.0.0.5", 11434, "llama3.2", false);
  CHECK(!p.secure());
  CHECK_EQ(static_cast<int>(p.streamFormat()), static_cast<int>(StreamFormat::NDJSON));
  ChatOptions o;
  MessageList m{Message::user("hi")};
  HttpRequest req;
  p.buildChatRequest(m, o, false, req);
  CHECK_STR_EQ(req.host, "10.0.0.5");
  CHECK_EQ(req.port, static_cast<uint16_t>(11434));
  CHECK_STR_EQ(req.path, "/api/chat");
  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(std::string(d["model"].as<const char*>()), "llama3.2");
  CHECK(!d["stream"].as<bool>());
}

TEST(ollama_parses_response_and_ndjson_stream) {
  OllamaProvider p;
  HttpResponse resp;
  resp.status = 200;
  resp.body = R"({"message":{"content":"Hi"},"done":true,"prompt_eval_count":7,"eval_count":3})";
  ChatResult cr;
  CHECK(p.parseChatResponse(resp, cr).isOk());
  CHECK_STR_EQ(cr.text, "Hi");
  CHECK_EQ(cr.outputTokens, static_cast<uint32_t>(3));

  StreamDelta d1;
  p.parseStreamEvent(R"({"message":{"content":"H"},"done":false})", d1);
  CHECK_STR_EQ(d1.textDelta, "H");
  CHECK(!d1.done);
  StreamDelta d2;
  p.parseStreamEvent(R"({"message":{"content":""},"done":true,"eval_count":3})", d2);
  CHECK(d2.done);
  CHECK_EQ(d2.outputTokens, static_cast<uint32_t>(3));
}
