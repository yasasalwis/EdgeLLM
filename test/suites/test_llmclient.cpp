// End-to-end LLMClient tests over a scripted fake socket: structured generation,
// HTTP error mapping, schema-validation retry, and Conversation append.
#include <string>

#include "../../src/llm/LLMClient.h"
#include "../../src/llm/ResponseSchema.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../fakes/FakeConnection.h"
#include "../framework/edge_test.h"

using namespace edge;
using edgetest::FakeConnection;

namespace {
std::string http200(const std::string& body) {
  return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}
std::string httpStatus(int code, const std::string& reason, const std::string& body) {
  return "HTTP/1.1 " + std::to_string(code) + " " + reason +
         "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}
// An OpenAI structured response whose message.content is the JSON string `inner`.
std::string openAiStructured(const std::string& innerJsonEscaped) {
  return http200(R"({"choices":[{"message":{"content":")" + innerJsonEscaped +
                 R"("},"finish_reason":"stop"}]})");
}
ResponseSchema answerSchema() {
  ResponseSchema s("r");
  s.field("answer", ParamType::String, "the answer");  // required
  return s;
}
}  // namespace

TEST(llmclient_generate_returns_validated_object) {
  FakeConnection conn;
  conn.toSend = openAiStructured("{\\\"answer\\\":\\\"hello\\\"}");
  OpenAIProvider provider("sk-test", "gpt-x");
  LLMClient client(provider, conn);

  Result<StructuredResult> r = client.generate(answerSchema(), "be brief", "say hello");
  CHECK(r.isOk());
  CHECK_STR_EQ(r.value().getString("answer"), "hello");
  // The request actually went out with auth + structured response_format.
  CHECK(conn.written.find("Authorization: Bearer sk-test\r\n") != std::string::npos);
  CHECK(conn.written.find("response_format") != std::string::npos);
}

TEST(llmclient_generate_maps_http_401) {
  FakeConnection conn;
  conn.toSend = httpStatus(401, "Unauthorized", R"({"error":{"message":"bad key"}})");
  OpenAIProvider provider("sk-bad");
  LLMClient client(provider, conn);
  Result<StructuredResult> r = client.generate(answerSchema(), "", "hi");
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::Unauthorized);
}

TEST(llmclient_generate_retries_on_invalid_then_succeeds) {
  FakeConnection conn;
  // First response omits the required "answer" (invalid); second is valid.
  conn.responses = {openAiStructured("{}"), openAiStructured("{\\\"answer\\\":\\\"ok\\\"}")};
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);
  client.setStructuredRetries(1);  // 2 attempts total

  Result<StructuredResult> r = client.generate(answerSchema(), "", "hi");
  CHECK(r.isOk());
  CHECK_STR_EQ(r.value().getString("answer"), "ok");
}

TEST(llmclient_generate_fails_after_retries_exhausted) {
  FakeConnection conn;
  conn.responses = {openAiStructured("{}"), openAiStructured("{}")};  // both invalid
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);
  client.setStructuredRetries(1);

  Result<StructuredResult> r = client.generate(answerSchema(), "", "hi");
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::SchemaValidationFailed);
}

TEST(llmclient_generate_appends_json_to_conversation) {
  FakeConnection conn;
  conn.toSend = openAiStructured("{\\\"answer\\\":\\\"hi\\\"}");
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);

  Conversation convo;
  convo.setSystem("be friendly");
  convo.addUser("hello");
  Result<StructuredResult> r = client.generate(answerSchema(), convo);
  CHECK(r.isOk());
  CHECK_EQ(convo.size(), static_cast<size_t>(2));  // user + assistant(json)
  CHECK_EQ(convo.messages().back().role, Role::Assistant);
  CHECK(convo.messages().back().content.find("\"answer\"") != std::string::npos);
}
