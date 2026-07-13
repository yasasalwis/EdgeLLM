// Tests for multimodal (image) input: each provider's native serialization of
// an image-carrying user message, plus the plain-text path staying unchanged.
#include <string>

#include <ArduinoJson.h>

#include "../../src/llm/LLMClient.h"
#include "../../src/llm/providers/AnthropicProvider.h"
#include "../../src/llm/providers/GeminiProvider.h"
#include "../../src/llm/providers/OllamaProvider.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../fakes/FakeConnection.h"
#include "../framework/edge_test.h"

using namespace edge;
using edgetest::FakeConnection;

namespace {
const char* kB64 = "AAECAwQ=";  // tiny stand-in payload

MessageList visionMessages() {
  return MessageList{Message::userWithImage("what is this?", kB64, "image/jpeg")};
}
ResponseSchema schema() {
  ResponseSchema s("r");
  s.field("label", ParamType::String, "");
  return s;
}
}  // namespace

TEST(message_user_with_image_factory) {
  Message m = Message::userWithImage("hi", kB64, "image/png");
  CHECK(m.hasImage());
  CHECK_EQ(m.role, Role::User);
  CHECK_STR_EQ(m.imageMime, "image/png");
  CHECK(!Message::user("hi").hasImage());
}

TEST(anthropic_serializes_image_content_blocks) {
  AnthropicProvider p("key");
  HttpRequest req;
  CHECK(p.buildStructuredRequest(visionMessages(), ChatOptions(), schema(), req).isOk());
  JsonDocument d;
  CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
  JsonArrayConst content = d["messages"][0]["content"];
  CHECK(!content.isNull());
  CHECK_STR_EQ(std::string(content[0]["type"].as<const char*>()), "image");
  CHECK_STR_EQ(std::string(content[0]["source"]["type"].as<const char*>()), "base64");
  CHECK_STR_EQ(std::string(content[0]["source"]["media_type"].as<const char*>()), "image/jpeg");
  CHECK_STR_EQ(std::string(content[0]["source"]["data"].as<const char*>()), kB64);
  CHECK_STR_EQ(std::string(content[1]["type"].as<const char*>()), "text");
  CHECK_STR_EQ(std::string(content[1]["text"].as<const char*>()), "what is this?");
}

TEST(anthropic_plain_message_stays_string_content) {
  AnthropicProvider p("key");
  HttpRequest req;
  MessageList msgs{Message::user("hello")};
  CHECK(p.buildStructuredRequest(msgs, ChatOptions(), schema(), req).isOk());
  JsonDocument d;
  CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
  CHECK(d["messages"][0]["content"].is<const char*>());  // no wasteful block array
}

TEST(openai_serializes_data_uri_image_url) {
  OpenAIProvider p("key");
  HttpRequest req;
  CHECK(p.buildStructuredRequest(visionMessages(), ChatOptions(), schema(), req).isOk());
  JsonDocument d;
  CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
  JsonArrayConst content = d["messages"][0]["content"];
  CHECK(!content.isNull());
  CHECK_STR_EQ(std::string(content[0]["type"].as<const char*>()), "text");
  CHECK_STR_EQ(std::string(content[1]["type"].as<const char*>()), "image_url");
  const std::string url = content[1]["image_url"]["url"].as<const char*>();
  CHECK_STR_EQ(url, std::string("data:image/jpeg;base64,") + kB64);
}

TEST(gemini_serializes_inline_data_part) {
  GeminiProvider p("key");
  HttpRequest req;
  CHECK(p.buildStructuredRequest(visionMessages(), ChatOptions(), schema(), req).isOk());
  JsonDocument d;
  CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
  JsonArrayConst parts = d["contents"][0]["parts"];
  CHECK_STR_EQ(std::string(parts[0]["text"].as<const char*>()), "what is this?");
  CHECK_STR_EQ(std::string(parts[1]["inline_data"]["mime_type"].as<const char*>()), "image/jpeg");
  CHECK_STR_EQ(std::string(parts[1]["inline_data"]["data"].as<const char*>()), kB64);
}

TEST(ollama_serializes_images_array) {
  OllamaProvider p;
  HttpRequest req;
  CHECK(p.buildStructuredRequest(visionMessages(), ChatOptions(), schema(), req).isOk());
  JsonDocument d;
  CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
  JsonArrayConst msgs = d["messages"];
  CHECK_STR_EQ(std::string(msgs[0]["images"][0].as<const char*>()), kB64);
  CHECK_STR_EQ(std::string(msgs[0]["content"].as<const char*>()), "what is this?");
}

TEST(image_only_message_omits_empty_text_part) {
  AnthropicProvider a("key");
  HttpRequest req;
  MessageList msgs{Message::userWithImage("", kB64, "image/jpeg")};
  CHECK(a.buildStructuredRequest(msgs, ChatOptions(), schema(), req).isOk());
  JsonDocument d;
  CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
  CHECK_EQ(d["messages"][0]["content"].as<JsonArrayConst>().size(), static_cast<size_t>(1));
}

TEST(llmclient_generate_with_image_end_to_end) {
  FakeConnection conn;
  const std::string body =
      R"({"choices":[{"message":{"content":"{\"label\":\"cat\"}"},"finish_reason":"stop"}]})";
  conn.toSend =
      "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;

  OpenAIProvider provider("k");
  LLMClient client(provider, conn);
  Result<StructuredResult> r =
      client.generate(schema(), "identify objects", "what is this?", kB64, "image/jpeg");
  CHECK(r.isOk());
  CHECK_STR_EQ(r.value().getString("label"), "cat");
  CHECK(conn.written.find("data:image/jpeg;base64,") != std::string::npos);
}
