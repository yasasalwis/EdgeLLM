// Tests for UsageMeter budget enforcement, ClientMetrics counters, and the new
// ChatOptions knobs (topP, stopSequences, extraHeaders) reaching the wire.
#include <string>

#include <ArduinoJson.h>

#include "../../src/llm/LLMClient.h"
#include "../../src/llm/UsageMeter.h"
#include "../../src/llm/providers/AnthropicProvider.h"
#include "../../src/llm/providers/GeminiProvider.h"
#include "../../src/llm/providers/OllamaProvider.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../fakes/FakeConnection.h"
#include "../framework/edge_test.h"

using namespace edge;
using edgetest::FakeConnection;

namespace {
std::string ok200(const std::string& usageJson = R"({"prompt_tokens":10,"completion_tokens":5})") {
  const std::string body = R"({"choices":[{"message":{"content":"{\"answer\":\"x\"}"},)"
                           R"("finish_reason":"stop"}],"usage":)" +
                           usageJson + "}";
  return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}
ResponseSchema answerSchema() {
  ResponseSchema s("r");
  s.field("answer", ParamType::String, "");
  return s;
}
}  // namespace

TEST(usage_meter_caps_requests) {
  UsageMeter m;
  m.setMaxRequests(2);
  CHECK(m.allowRequest());
  m.recordRequest();
  m.recordRequest();
  CHECK(!m.allowRequest());
  m.reset();
  CHECK(m.allowRequest());
}

TEST(usage_meter_caps_tokens) {
  UsageMeter m;
  m.setMaxTotalTokens(100);
  m.recordTokens(60, 39);
  CHECK(m.allowRequest());  // 99 < 100
  m.recordTokens(1, 0);
  CHECK(!m.allowRequest());
  CHECK_EQ(m.totalTokens(), 100u);
}

TEST(llmclient_enforces_usage_budget) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {ok200(), ok200()};
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);
  UsageMeter meter;
  meter.setMaxRequests(1);
  client.setUsageMeter(&meter);

  CHECK(client.generate(answerSchema(), "", "one").isOk());
  Result<StructuredResult> second = client.generate(answerSchema(), "", "two");
  CHECK(!second.isOk());
  CHECK_EQ(second.error(), Error::BudgetExceeded);
  CHECK_EQ(meter.requests(), 1u);
  CHECK_EQ(meter.inputTokens(), 10u);  // provider-reported usage was recorded
  CHECK_EQ(meter.outputTokens(), 5u);
}

TEST(llmclient_metrics_count_requests_and_tokens) {
  FakeConnection conn;
  conn.toSend = ok200();
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);

  CHECK(client.generate(answerSchema(), "", "hi").isOk());
  const ClientMetrics& m = client.metrics();
  CHECK_EQ(m.requests, 1u);
  CHECK_EQ(m.retries, 0u);
  CHECK_EQ(m.httpErrors, 0u);
  CHECK_EQ(m.inputTokens, 10u);
  CHECK_EQ(m.outputTokens, 5u);

  client.resetMetrics();
  CHECK_EQ(client.metrics().requests, 0u);
}

TEST(llmclient_metrics_count_http_errors) {
  FakeConnection conn;
  const std::string body = R"({"error":{"message":"nope"}})";
  conn.toSend = "HTTP/1.1 401 Unauthorized\r\nContent-Length: " + std::to_string(body.size()) +
                "\r\n\r\n" + body;
  OpenAIProvider provider("bad");
  LLMClient client(provider, conn);

  CHECK(!client.generate(answerSchema(), "", "hi").isOk());
  CHECK_EQ(client.metrics().httpErrors, 1u);
  CHECK_EQ(client.metrics().requests, 1u);
}

TEST(chat_options_sampling_reaches_all_providers) {
  ChatOptions opts;
  opts.topP = 0.9f;
  opts.stopSequences.push_back("END");
  MessageList msgs{Message::user("hi")};
  ResponseSchema schema = answerSchema();

  {
    AnthropicProvider p("k");
    HttpRequest req;
    CHECK(p.buildStructuredRequest(msgs, opts, schema, req).isOk());
    JsonDocument d;
    CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
    CHECK(d["top_p"].as<float>() > 0.89f);
    CHECK_STR_EQ(std::string(d["stop_sequences"][0].as<const char*>()), "END");
  }
  {
    OpenAIProvider p("k");
    HttpRequest req;
    CHECK(p.buildStructuredRequest(msgs, opts, schema, req).isOk());
    JsonDocument d;
    CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
    CHECK(d["top_p"].as<float>() > 0.89f);
    CHECK_STR_EQ(std::string(d["stop"][0].as<const char*>()), "END");
  }
  {
    GeminiProvider p("k");
    HttpRequest req;
    CHECK(p.buildStructuredRequest(msgs, opts, schema, req).isOk());
    JsonDocument d;
    CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
    CHECK(d["generationConfig"]["topP"].as<float>() > 0.89f);
    CHECK_STR_EQ(std::string(d["generationConfig"]["stopSequences"][0].as<const char*>()), "END");
  }
  {
    OllamaProvider p;
    HttpRequest req;
    CHECK(p.buildStructuredRequest(msgs, opts, schema, req).isOk());
    JsonDocument d;
    CHECK(deserializeJson(d, req.body) == DeserializationError::Ok);
    CHECK(d["options"]["top_p"].as<float>() > 0.89f);
    CHECK_STR_EQ(std::string(d["options"]["stop"][0].as<const char*>()), "END");
  }
}

TEST(chat_options_extra_headers_sent) {
  FakeConnection conn;
  conn.toSend = ok200();
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);
  client.options().extraHeaders.emplace_back("X-Title", "EdgeLLM-Device");

  CHECK(client.generate(answerSchema(), "", "hi").isOk());
  CHECK(conn.written.find("X-Title: EdgeLLM-Device\r\n") != std::string::npos);
}
