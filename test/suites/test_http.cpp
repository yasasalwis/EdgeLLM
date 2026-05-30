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
