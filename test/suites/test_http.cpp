// Tests for HTTP value types and request serialization.
#include "../../src/transport/HttpRequestBuilder.h"
#include "../../src/transport/HttpTypes.h"
#include "../framework/edge_test.h"

using namespace edge;

TEST(header_name_match_is_case_insensitive) {
  CHECK(headerNameEquals("Content-Type", "content-type"));
  CHECK(headerNameEquals("X-Api-Key", "x-api-key"));
  CHECK(!headerNameEquals("Content-Type", "Content-Length"));
}

TEST(request_set_header_replaces_in_place) {
  HttpRequest req;
  req.setHeader("Accept", "text/plain");
  req.setHeader("accept", "application/json");  // case-insensitive replace
  CHECK_EQ(req.headers.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(*req.header("Accept"), "application/json");
}

TEST(serialize_get_adds_host_and_connection_close) {
  HttpRequest req;
  req.method = "GET";
  req.host = "api.example.com";
  req.path = "/v1/health";
  const std::string wire = serializeRequest(req);
  CHECK(wire.find("GET /v1/health HTTP/1.1\r\n") == 0);
  CHECK(wire.find("Host: api.example.com\r\n") != std::string::npos);
  CHECK(wire.find("Connection: close\r\n") != std::string::npos);
  CHECK(wire.find("\r\n\r\n") != std::string::npos);
}

TEST(serialize_post_adds_content_length) {
  HttpRequest req;
  req.method = "POST";
  req.host = "api.example.com";
  req.path = "/v1/messages";
  req.body = "{\"hi\":true}";
  req.setHeader("Content-Type", "application/json");
  const std::string wire = serializeRequest(req);
  CHECK(wire.find("Content-Length: 11\r\n") != std::string::npos);
  CHECK(wire.find("Content-Type: application/json\r\n") != std::string::npos);
  // Body present after the blank line.
  const size_t bodyAt = wire.find("\r\n\r\n");
  CHECK(bodyAt != std::string::npos);
  CHECK_STR_EQ(wire.substr(bodyAt + 4), "{\"hi\":true}");
}

TEST(serialize_respects_explicit_content_length_and_chunked) {
  HttpRequest req;
  req.host = "h";
  req.body = "abc";
  req.setHeader("Transfer-Encoding", "chunked");
  const std::string wire = serializeRequest(req);
  // Chunked framing means we must NOT add Content-Length.
  CHECK(wire.find("Content-Length:") == std::string::npos);
}

TEST(serialize_defaults_empty_path_to_root) {
  HttpRequest req;
  req.host = "h";
  req.path = "";
  const std::string wire = serializeRequest(req);
  CHECK(wire.find("GET / HTTP/1.1\r\n") == 0);
}

TEST(serialize_strips_crlf_to_prevent_header_injection) {
  HttpRequest req;
  req.host = "h";
  req.method = "GET";
  // An injected header value attempting to add a forged header + smuggled body.
  req.addHeader("X-Evil", "value\r\nX-Injected: 1\r\n\r\nGET /smuggled HTTP/1.1");
  const std::string wire = serializeRequest(req);
  // No forged header LINE and no smuggled request LINE exist (the CR/LF that
  // would create them are stripped). The literal text may remain harmlessly
  // inside the single sanitized value.
  CHECK(wire.find("\r\nX-Injected:") == std::string::npos);
  CHECK(wire.find("\r\nGET /smuggled") == std::string::npos);
  // The header is still emitted, just collapsed onto one line.
  CHECK(wire.find("X-Evil: valueX-Injected: 1GET /smuggled HTTP/1.1\r\n") != std::string::npos);
}

TEST(serialize_strips_crlf_in_path) {
  HttpRequest req;
  req.host = "h";
  req.path = "/ok\r\nHost: evil.com";
  const std::string wire = serializeRequest(req);
  CHECK(wire.find("GET /okHost: evil.com HTTP/1.1\r\n") == 0);
  // No second Host line was injected by the path.
  CHECK(wire.find("\r\nHost: evil.com\r\n") == std::string::npos);
}
