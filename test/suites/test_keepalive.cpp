// Tests for HTTP keep-alive: connection reuse across requests, server-initiated
// close, stale-connection retry, unframed-body fallback, and the LLMClient
// connection lifecycle (per-cycle reuse, persistent mode).
#include <string>

#include "../../src/llm/LLMClient.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../../src/transport/HttpClient.h"
#include "../../src/transport/HttpRequestBuilder.h"
#include "../fakes/FakeConnection.h"
#include "../framework/edge_test.h"

using namespace edge;
using edgetest::FakeConnection;

namespace {
std::string http200(const std::string& body, const std::string& extraHeaders = "") {
  return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" +
         extraHeaders + "\r\n" + body;
}
HttpRequest getReq() {
  HttpRequest req;
  req.method = "GET";
  req.host = "example.com";
  req.path = "/";
  return req;
}
std::string openAiStructured(const std::string& innerJsonEscaped) {
  return http200(R"({"choices":[{"message":{"content":")" + innerJsonEscaped +
                 R"("},"finish_reason":"stop"}]})");
}
}  // namespace

TEST(serialize_request_keepalive_default) {
  HttpRequest req = getReq();
  CHECK(serializeRequest(req).find("Connection: close\r\n") != std::string::npos);
  CHECK(serializeRequest(req, true).find("Connection: keep-alive\r\n") != std::string::npos);
  // An explicit caller header always wins.
  req.setHeader("Connection", "close");
  CHECK(serializeRequest(req, true).find("Connection: close\r\n") != std::string::npos);
}

TEST(httpclient_keepalive_reuses_connection) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {http200("one"), http200("two")};
  HttpClient http(conn);
  http.setKeepAlive(true);

  HttpResponse r1, r2;
  CHECK(http.send(getReq(), r1).isOk());
  CHECK_STR_EQ(r1.body, "one");
  CHECK(http.connectionReusable());
  CHECK(http.send(getReq(), r2).isOk());
  CHECK_STR_EQ(r2.body, "two");
  CHECK_EQ(conn.connectCount, 1);  // the whole point: one handshake, two requests
  CHECK(conn.written.find("Connection: keep-alive\r\n") != std::string::npos);
}

TEST(httpclient_honors_server_connection_close) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {http200("one", "Connection: close\r\n"), http200("two")};
  HttpClient http(conn);
  http.setKeepAlive(true);

  HttpResponse r1, r2;
  CHECK(http.send(getReq(), r1).isOk());
  CHECK(!http.connectionReusable());
  CHECK(!conn.connected());  // client closed it because the server said so
  CHECK(http.send(getReq(), r2).isOk());
  CHECK_EQ(conn.connectCount, 2);  // second request needed a fresh connection
  CHECK_STR_EQ(r2.body, "two");
}

TEST(httpclient_retries_once_on_stale_connection) {
  FakeConnection conn;
  conn.staleAfterDrain = true;  // after a response drains, reads fail but connected() stays true
  conn.responses = {http200("one"), http200("two")};
  HttpClient http(conn);
  http.setKeepAlive(true);

  HttpResponse r1, r2;
  CHECK(http.send(getReq(), r1).isOk());
  CHECK_STR_EQ(r1.body, "one");
  // The server has silently dropped the connection; the client must notice the
  // dead reuse, reconnect, and replay the request.
  CHECK(http.send(getReq(), r2).isOk());
  CHECK_STR_EQ(r2.body, "two");
  CHECK_EQ(conn.connectCount, 2);
}

TEST(httpclient_unframed_body_not_reusable) {
  FakeConnection conn;
  // No Content-Length / chunked: body runs until close.
  conn.toSend = "HTTP/1.1 200 OK\r\n\r\nraw-until-close";
  HttpClient http(conn);
  http.setKeepAlive(true);

  HttpResponse r;
  CHECK(http.send(getReq(), r).isOk());
  CHECK_STR_EQ(r.body, "raw-until-close");
  CHECK(!http.connectionReusable());
  CHECK(!conn.connected());
}

TEST(httpclient_content_length_never_reads_past_body) {
  FakeConnection conn;
  conn.closeWhenDrained = false;
  // Both responses arrive concatenated (worst case for over-read).
  conn.toSend = http200("first") + http200("second");
  HttpClient http(conn);
  http.setKeepAlive(true);

  HttpResponse r1, r2;
  CHECK(http.send(getReq(), r1).isOk());
  CHECK_STR_EQ(r1.body, "first");
  CHECK(http.send(getReq(), r2).isOk());
  CHECK_STR_EQ(r2.body, "second");  // second response intact, nothing discarded
  CHECK_EQ(conn.connectCount, 1);
}

TEST(llmclient_agent_cycle_reuses_connection_then_closes) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  // Round 1: model requests a tool; round 2: model settles; then the final
  // structured generate. Three requests over one connection.
  conn.responses = {
      http200(
          R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"c1","type":"function","function":{"name":"get_temp","arguments":"{}"}}]},"finish_reason":"tool_calls"}]})"),
      http200(R"({"choices":[{"message":{"content":"21 C"},"finish_reason":"stop"}]})"),
      openAiStructured("{\\\"answer\\\":\\\"21\\\"}"),
  };
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);

  ToolRegistry tools;
  tools.addTool("get_temp", "Read temperature").onCall([](ToolCallArgs&) {
    return ToolResult::ok("21");
  });
  ResponseSchema schema("r");
  schema.field("answer", ParamType::String, "");

  Result<StructuredResult> r = client.run(schema, "temp?", tools);
  CHECK(r.isOk());
  CHECK_STR_EQ(r.value().getString("answer"), "21");
  CHECK_EQ(conn.connectCount, 1);  // all three requests reused one connection
  CHECK(!conn.connected());        // and the cycle closed it at the end
}

TEST(llmclient_persistent_mode_keeps_connection_open) {
  FakeConnection conn;
  conn.keepAliveServer = true;
  conn.closeWhenDrained = false;
  conn.responses = {openAiStructured("{\\\"answer\\\":\\\"a\\\"}"),
                    openAiStructured("{\\\"answer\\\":\\\"b\\\"}")};
  OpenAIProvider provider("k");
  LLMClient client(provider, conn);
  client.setPersistentConnection(true);

  ResponseSchema schema("r");
  schema.field("answer", ParamType::String, "");
  CHECK(client.generate(schema, "", "one").isOk());
  CHECK(conn.connected());  // still open between calls
  CHECK(client.generate(schema, "", "two").isOk());
  CHECK_EQ(conn.connectCount, 1);  // second call reused it
}
