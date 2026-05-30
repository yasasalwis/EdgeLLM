// End-to-end LLMClient tests over a scripted fake socket: blocking chat,
// HTTP error mapping, SSE streaming (OpenAI), NDJSON streaming (Ollama), and
// Conversation append.
#include <string>

#include "../../src/llm/LLMClient.h"
#include "../../src/llm/providers/OllamaProvider.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../fakes/FakeConnection.h"
#include "../framework/edge_test.h"

using namespace edge;
using edgetest::FakeConnection;

namespace {
std::string httpWithLength(int status, const std::string& reason, const std::string& body) {
  return "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\nContent-Length: " +
         std::to_string(body.size()) + "\r\n\r\n" + body;
}
std::string toHex(size_t n) {
  if (n == 0) return "0";
  const char* h = "0123456789abcdef";
  std::string s;
  while (n > 0) {
    s.insert(s.begin(), h[n & 0xF]);
    n >>= 4;
  }
  return s;
}
// Wraps a body as a single HTTP chunked-transfer response.
std::string httpChunked(const std::string& body) {
  return "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n" + toHex(body.size()) + "\r\n" +
         body + "\r\n0\r\n\r\n";
}
}  // namespace

TEST(llmclient_blocking_chat_openai) {
  FakeConnection conn;
  conn.toSend = httpWithLength(
      200, "OK",
      R"({"choices":[{"message":{"content":"Hello from AI"},"finish_reason":"stop"}],"usage":{"prompt_tokens":5,"completion_tokens":3}})");
  OpenAIProvider provider("sk-test", "gpt-x");
  LLMClient client(provider, conn);

  Result<ChatResult> r = client.chat("hi there");
  CHECK(r.isOk());
  CHECK_STR_EQ(r.value().text, "Hello from AI");
  CHECK_EQ(r.value().outputTokens, static_cast<uint32_t>(3));
  // The request actually went out with auth + correct route.
  CHECK(conn.written.find("POST /v1/chat/completions HTTP/1.1\r\n") == 0);
  CHECK(conn.written.find("Authorization: Bearer sk-test\r\n") != std::string::npos);
}

TEST(llmclient_maps_http_401_to_unauthorized) {
  FakeConnection conn;
  conn.toSend = httpWithLength(401, "Unauthorized", R"({"error":{"message":"bad key"}})");
  OpenAIProvider provider("sk-bad");
  LLMClient client(provider, conn);
  Result<ChatResult> r = client.chat("hi");
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::Unauthorized);
}

TEST(llmclient_maps_http_429_to_rate_limited) {
  FakeConnection conn;
  conn.toSend = httpWithLength(429, "Too Many Requests", R"({"error":{"message":"slow down"}})");
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);
  CHECK_EQ(client.chat("hi").error(), Error::RateLimited);
}

TEST(llmclient_streaming_openai_sse) {
  FakeConnection conn;
  std::string sse =
      "data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}\n\n"
      "data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}]}\n\n"
      "data: [DONE]\n\n";
  conn.toSend = httpChunked(sse);
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);

  std::string collected;
  ChatResult final;
  Status s = client.chatStream(
      "hi", [&](const std::string& d) { collected += d; }, &final);
  CHECK(s.isOk());
  CHECK_STR_EQ(collected, "Hello");
  CHECK_STR_EQ(final.text, "Hello");
}

TEST(llmclient_streaming_ollama_ndjson) {
  FakeConnection conn;
  std::string nd =
      "{\"message\":{\"content\":\"Hel\"},\"done\":false}\n"
      "{\"message\":{\"content\":\"lo\"},\"done\":false}\n"
      "{\"message\":{\"content\":\"\"},\"done\":true,\"eval_count\":2,\"prompt_eval_count\":3}\n";
  // No Content-Length / not chunked: stream until the server closes.
  conn.toSend = "HTTP/1.1 200 OK\r\n\r\n" + nd;
  conn.closeWhenDrained = true;
  OllamaProvider provider;
  LLMClient client(provider, conn);

  std::string collected;
  ChatResult final;
  Status s = client.chatStream(
      "hi", [&](const std::string& d) { collected += d; }, &final);
  CHECK(s.isOk());
  CHECK_STR_EQ(collected, "Hello");
  CHECK_EQ(final.outputTokens, static_cast<uint32_t>(2));
}

TEST(llmclient_chat_appends_to_conversation) {
  FakeConnection conn;
  conn.toSend = httpWithLength(
      200, "OK", R"({"choices":[{"message":{"content":"Hi!"},"finish_reason":"stop"}]})");
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);

  Conversation convo;
  convo.setSystem("be friendly");
  convo.addUser("hello");
  Result<ChatResult> r = client.chat(convo);
  CHECK(r.isOk());
  // user + assistant now in history.
  CHECK_EQ(convo.size(), static_cast<size_t>(2));
  CHECK_EQ(convo.messages().back().role, Role::Assistant);
  CHECK_STR_EQ(convo.messages().back().content, "Hi!");
  // system prompt was sent.
  CHECK(conn.written.find("be friendly") != std::string::npos);
}
