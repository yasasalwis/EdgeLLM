// Tests for the Arduino-independent core: Result/Status, error strings,
// capability detection, and the redacting logger.
#include "../../src/core/Capabilities.h"
#include "../../src/core/Errors.h"
#include "../../src/core/Logger.h"
#include "../../src/core/Result.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
struct CaptureSink : ILogSink {
  std::vector<std::string> lines;
  void write(LogLevel, const std::string& line) override { lines.push_back(line); }
};
}  // namespace

TEST(status_ok_and_fail) {
  CHECK(Status::ok().isOk());
  CHECK(!Status::fail(Error::Timeout).isOk());
  CHECK_EQ(Status::fail(Error::Timeout).error(), Error::Timeout);
  CHECK(static_cast<bool>(Status::ok()));
  CHECK(!static_cast<bool>(Status::fail(Error::NotFound)));
}

TEST(result_carries_value_or_error) {
  auto good = Result<int>::ok(42);
  CHECK(good.isOk());
  CHECK_EQ(good.value(), 42);
  CHECK_EQ(good.valueOr(0), 42);

  auto bad = Result<int>::fail(Error::SecretNotFound);
  CHECK(!bad.isOk());
  CHECK_EQ(bad.error(), Error::SecretNotFound);
  CHECK_EQ(bad.valueOr(7), 7);
}

TEST(error_strings_are_stable_and_safe) {
  CHECK_STR_EQ(errorString(Error::Ok), "ok");
  CHECK_STR_EQ(errorString(Error::CertVerifyFailed), "certificate verification failed");
  CHECK_STR_EQ(errorString(Error::McpWriteNotAllowed), "mcp write not allowed");
}

TEST(capabilities_native_profile_is_sane) {
  Capabilities c = detectCapabilities();
  CHECK(c.board != nullptr);
  CHECK(c.maxTlsConnections >= 1);
  CHECK(c.recvBufferSize > 0);
  CHECK(c.maxResponseBody > 0);
  // On the host build we expect the "native" profile.
  CHECK_STR_EQ(c.board, "native");
  CHECK(c.supportsFullDuplex);
}

TEST(logger_filters_below_level) {
  CaptureSink sink;
  Logger log;
  log.setSink(&sink);
  log.setLevel(LogLevel::Warn);
  log.info("noisy");    // dropped
  log.warn("careful");  // kept
  log.error("boom");    // kept
  CHECK_EQ(sink.lines.size(), static_cast<size_t>(2));
  CHECK(sink.lines[0].find("careful") != std::string::npos);
  CHECK(sink.lines[0].find("[WARN]") != std::string::npos);
}

TEST(logger_redacts_registered_secrets) {
  CaptureSink sink;
  Logger log;
  log.setSink(&sink);
  log.setLevel(LogLevel::Debug);
  log.registerSecret("sk-ant-supersecret-9999");
  log.info("calling api with key sk-ant-supersecret-9999 now");
  CHECK_EQ(sink.lines.size(), static_cast<size_t>(1));
  CHECK(sink.lines[0].find("supersecret") == std::string::npos);
  CHECK(sink.lines[0].find("***") != std::string::npos);
}

TEST(logger_ignores_trivial_secrets) {
  Logger log;
  log.registerSecret("ab");  // too short, ignored
  CHECK_STR_EQ(log.redact("abc"), "abc");
}

TEST(mask_token_hides_length_and_body) {
  CHECK_STR_EQ(Logger::maskToken("sk-ant-api03-abcd1234"), "***1234");
  CHECK_STR_EQ(Logger::maskToken("short", 4), "***hort");
  CHECK_STR_EQ(Logger::maskToken("abc", 4), "***");
  CHECK_STR_EQ(Logger::maskToken(""), "***");
}
