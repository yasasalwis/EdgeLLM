// Tests for the LLMClient retry policy: transient HTTP statuses (429/5xx),
// transient transport errors, Retry-After, backoff growth, jitter bounds, and
// the errors that must never be retried.
#include <string>
#include <vector>

#include "../../src/llm/LLMClient.h"
#include "../../src/llm/ResponseSchema.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../fakes/FakeConnection.h"
#include "../framework/edge_test.h"

using namespace edge;
using edgetest::FakeConnection;

namespace {
std::vector<uint32_t>& recordedDelays() {
  static std::vector<uint32_t> v;
  return v;
}
void recordDelay(uint32_t ms) { recordedDelays().push_back(ms); }

std::string httpWith(int code, const std::string& reason, const std::string& body,
                     const std::string& extraHeaders = "") {
  return "HTTP/1.1 " + std::to_string(code) + " " + reason +
         "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + extraHeaders + "\r\n" +
         body;
}
std::string ok200() {
  const std::string inner = "{\\\"answer\\\":\\\"hi\\\"}";
  return httpWith(
      200, "OK",
      R"({"choices":[{"message":{"content":")" + inner + R"("},"finish_reason":"stop"}]})");
}
ResponseSchema answerSchema() {
  ResponseSchema s("r");
  s.field("answer", ParamType::String, "");
  return s;
}
size_t countRequests(const std::string& written) {
  size_t n = 0, pos = 0;
  while ((pos = written.find("POST ", pos)) != std::string::npos) {
    ++n;
    pos += 5;
  }
  return n;
}

// A client wired for deterministic retry tests: recording delay fn, no jitter.
LLMClient makeClient(Provider& p, IConnection& c, bool jitter = false) {
  LLMClient client(p, c);
  client.setDelayFn(recordDelay);
  client.retryPolicy().jitter = jitter;
  recordedDelays().clear();
  return client;
}
}  // namespace

TEST(retry_recovers_from_500_then_succeeds) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {httpWith(500, "Internal", R"({"error":{"message":"boom"}})"), ok200()};
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn);

  Result<StructuredResult> r = client.generate(answerSchema(), "", "hi");
  CHECK(r.isOk());
  CHECK_EQ(recordedDelays().size(), static_cast<size_t>(1));
  CHECK_EQ(recordedDelays()[0], 500u);  // initial backoff, no jitter
}

TEST(retry_respects_retry_after_seconds) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {httpWith(429, "Too Many", "{}", "Retry-After: 2\r\n"), ok200()};
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn);

  CHECK(client.generate(answerSchema(), "", "hi").isOk());
  CHECK_EQ(recordedDelays().size(), static_cast<size_t>(1));
  CHECK_EQ(recordedDelays()[0], 2000u);  // server-directed wait
}

TEST(retry_caps_retry_after_at_max_backoff) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {httpWith(429, "Too Many", "{}", "Retry-After: 3600\r\n"), ok200()};
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn);

  CHECK(client.generate(answerSchema(), "", "hi").isOk());
  CHECK_EQ(recordedDelays()[0], client.retryPolicy().maxBackoffMs);
}

TEST(retry_exhaustion_maps_rate_limited_with_growing_backoff) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {httpWith(429, "Too Many", "{}"), httpWith(429, "Too Many", "{}"),
                    httpWith(429, "Too Many", "{}")};
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn);  // maxRetries default 2 -> 3 attempts

  Result<StructuredResult> r = client.generate(answerSchema(), "", "hi");
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::RateLimited);
  CHECK_EQ(recordedDelays().size(), static_cast<size_t>(2));
  CHECK_EQ(recordedDelays()[0], 500u);
  CHECK_EQ(recordedDelays()[1], 1000u);  // doubled
  CHECK_EQ(countRequests(conn.written), static_cast<size_t>(3));
}

TEST(retry_recovers_from_transient_connect_failure) {
  FakeConnection conn;
  conn.failFirstConnects = 1;
  conn.toSend = ok200();
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn);

  CHECK(client.generate(answerSchema(), "", "hi").isOk());
  CHECK_EQ(recordedDelays().size(), static_cast<size_t>(1));
}

TEST(no_retry_on_client_error_status) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {httpWith(400, "Bad Request", R"({"error":{"message":"bad"}})"), ok200()};
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn);

  Result<StructuredResult> r = client.generate(answerSchema(), "", "hi");
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::InvalidArgument);
  CHECK(recordedDelays().empty());
  CHECK_EQ(countRequests(conn.written), static_cast<size_t>(1));
}

TEST(no_retry_on_certificate_failure) {
  FakeConnection conn;
  conn.connectResult = Status::fail(Error::CertVerifyFailed);
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn);

  Result<StructuredResult> r = client.generate(answerSchema(), "", "hi");
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::CertVerifyFailed);
  CHECK(recordedDelays().empty());
}

TEST(retry_disabled_fails_immediately) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {httpWith(503, "Unavailable", "{}"), ok200()};
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn);
  client.retryPolicy().maxRetries = 0;

  Result<StructuredResult> r = client.generate(answerSchema(), "", "hi");
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::ProviderError);
  CHECK(recordedDelays().empty());
}

TEST(retry_jitter_stays_within_bounds) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {httpWith(500, "Internal", "{}"), ok200()};
  OpenAIProvider provider("k");
  LLMClient client = makeClient(provider, conn, /*jitter=*/true);

  CHECK(client.generate(answerSchema(), "", "hi").isOk());
  CHECK_EQ(recordedDelays().size(), static_cast<size_t>(1));
  CHECK(recordedDelays()[0] >= 250u);  // [half, full] of the 500 ms base
  CHECK(recordedDelays()[0] <= 500u);
}
