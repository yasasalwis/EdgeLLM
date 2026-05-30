// Tests for HttpClient over a scripted fake socket: status parsing, both body
// framings, byte-drip resilience, timeouts and the body-size guard.
#include "../../src/transport/HttpClient.h"
#include "../fakes/FakeConnection.h"
#include "../framework/edge_test.h"

using namespace edge;
using edgetest::FakeConnection;

TEST(http_content_length_response) {
  FakeConnection conn;
  conn.toSend = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
  HttpClient http(conn);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(s.isOk());
  CHECK_EQ(resp.status, 200);
  CHECK_STR_EQ(resp.reason, "OK");
  CHECK_STR_EQ(resp.body, "hello");
  CHECK(resp.isSuccess());
}

TEST(http_chunked_response) {
  FakeConnection conn;
  conn.toSend =
      "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n5\r\nworld\r\n0\r\n\r\n";
  HttpClient http(conn);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(s.isOk());
  CHECK_EQ(resp.status, 200);
  CHECK_STR_EQ(resp.body, "helloworld");
}

TEST(http_chunked_response_byte_drip) {
  FakeConnection conn;
  conn.toSend = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n";
  conn.chunkSize = 1;  // deliver one byte per read
  HttpClient http(conn);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(s.isOk());
  CHECK_STR_EQ(resp.body, "abc");
}

TEST(http_writes_serialized_request) {
  FakeConnection conn;
  conn.toSend = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
  HttpClient http(conn);
  HttpRequest req;
  req.method = "POST";
  req.host = "api.test";
  req.path = "/v1/ping";
  req.body = "hey";
  HttpResponse resp;
  http.send(req, resp);
  CHECK(conn.written.find("POST /v1/ping HTTP/1.1\r\n") == 0);
  CHECK(conn.written.find("Host: api.test\r\n") != std::string::npos);
  CHECK(conn.written.find("Content-Length: 3\r\n") != std::string::npos);
  CHECK(conn.written.find("\r\n\r\nhey") != std::string::npos);
}

TEST(http_error_status_still_parses_body) {
  FakeConnection conn;
  conn.toSend = "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nnot here!";
  HttpClient http(conn);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(s.isOk());  // a complete exchange, even though HTTP status is an error
  CHECK_EQ(resp.status, 404);
  CHECK(!resp.isSuccess());
  CHECK_STR_EQ(resp.body, "not here!");
}

TEST(http_204_has_no_body) {
  FakeConnection conn;
  conn.toSend = "HTTP/1.1 204 No Content\r\n\r\n";
  HttpClient http(conn);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(s.isOk());
  CHECK_EQ(resp.status, 204);
  CHECK(resp.body.empty());
}

TEST(http_read_until_close_when_no_framing) {
  FakeConnection conn;
  conn.toSend = "HTTP/1.1 200 OK\r\n\r\nbodybytes";
  conn.closeWhenDrained = true;
  HttpClient http(conn);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(s.isOk());
  CHECK_STR_EQ(resp.body, "bodybytes");
}

TEST(http_body_too_large_is_rejected) {
  FakeConnection conn;
  conn.toSend = "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n";
  HttpClient http(conn);
  http.setMaxResponseBody(10);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(!s.isOk());
  CHECK_EQ(s.error(), Error::HttpBodyTooLarge);
}

TEST(http_connect_failure_propagates) {
  FakeConnection conn;
  conn.connectResult = Status::fail(Error::ConnectFailed);
  HttpClient http(conn);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(!s.isOk());
  CHECK_EQ(s.error(), Error::ConnectFailed);
}

TEST(http_timeout_when_stalled) {
  FakeConnection conn;
  conn.stall = true;  // connected but never delivers data
  HttpClient http(conn);
  http.setClock(edgetest::fakeClockNow);  // auto-advancing clock
  http.setTimeout(5000);
  HttpResponse resp;
  Status s = http.send(HttpRequest{}, resp);
  CHECK(!s.isOk());
  CHECK_EQ(s.error(), Error::Timeout);
}
