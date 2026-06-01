// End-to-end agent-loop tests: the model requests a tool, the loop runs it,
// then produces a final STRUCTURED answer validated against a schema. Driven by
// a scripted multi-response fake socket.
#include <string>

#include "../../src/llm/LLMClient.h"
#include "../../src/llm/ResponseSchema.h"
#include "../../src/llm/providers/AnthropicProvider.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../../src/tools/ToolRegistry.h"
#include "../fakes/FakeConnection.h"
#include "../framework/edge_test.h"

using namespace edge;
using edgetest::FakeConnection;

namespace {
std::string http200(const std::string& body) {
  return "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

ToolRegistry adderRegistry() {
  ToolRegistry r;
  r.addTool("add", "Add two integers")
      .param("a", ParamType::Integer, "first", true)
      .param("b", ParamType::Integer, "second", true)
      .onCall([](ToolCallArgs& args) {
        return ToolResult::ok(std::to_string(args.getInt("a") + args.getInt("b")));
      });
  return r;
}

ResponseSchema sumSchema() {
  ResponseSchema s("answer");
  s.field("sum", ParamType::Integer, "the computed sum");
  return s;
}

// A provider that doesn't override the tool hooks (base default supportsTools()
// == false) — must still satisfy the structured pure-virtuals.
struct NoToolsProvider : public Provider {
  const char* name() const override { return "no-tools"; }
  bool secure() const override { return true; }
  Status buildStructuredRequest(const MessageList&, const ChatOptions&, const ResponseSchema&,
                                HttpRequest&) override {
    return Status::ok();
  }
  Status parseStructuredResponse(const HttpResponse&, std::string&, ChatResult&) override {
    return Status::ok();
  }
};
}  // namespace

TEST(agentloop_openai_runs_tool_then_structured_answer) {
  FakeConnection conn;
  conn.responses = {
      // 1) model requests the tool
      http200(
          R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"c1","type":"function","function":{"name":"add","arguments":"{\"a\":2,\"b\":3}"}}]},"finish_reason":"tool_calls"}]})"),
      // 2) model settles (no more tools)
      http200(R"({"choices":[{"message":{"content":"I have it."},"finish_reason":"stop"}]})"),
      // 3) final structured answer
      http200(R"({"choices":[{"message":{"content":"{\"sum\":5}"},"finish_reason":"stop"}]})"),
  };
  OpenAIProvider provider("k");
  ToolRegistry reg = adderRegistry();
  LLMClient client(provider, conn);

  Result<StructuredResult> r = client.run(sumSchema(), "add 2 and 3", reg);
  CHECK(r.isOk());
  CHECK_EQ(r.value().getInt("sum"), 5L);
  // The tool result was sent back during the loop.
  CHECK(conn.written.find("\"content\":\"5\"") != std::string::npos);
}

TEST(agentloop_anthropic_runs_tool_then_structured_answer) {
  FakeConnection conn;
  conn.responses = {
      // 1) tool_use request
      http200(
          R"({"content":[{"type":"tool_use","id":"t1","name":"add","input":{"a":4,"b":5}}],"stop_reason":"tool_use"})"),
      // 2) settle (text only, no tool_use)
      http200(R"({"content":[{"type":"text","text":"done"}],"stop_reason":"end_turn"})"),
      // 3) final structured answer (forced tool input)
      http200(
          R"({"content":[{"type":"tool_use","id":"t2","name":"answer","input":{"sum":9}}],"stop_reason":"tool_use"})"),
  };
  AnthropicProvider provider("k");
  ToolRegistry reg = adderRegistry();
  LLMClient client(provider, conn);

  Result<StructuredResult> r = client.run(sumSchema(), "add 4 and 5", reg);
  CHECK(r.isOk());
  CHECK_EQ(r.value().getInt("sum"), 9L);
}

TEST(agentloop_enforces_max_iterations) {
  FakeConnection conn;
  std::string toolReq = http200(
      R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"c","type":"function","function":{"name":"add","arguments":"{\"a\":1,\"b\":1}"}}]},"finish_reason":"tool_calls"}]})");
  conn.responses = {toolReq, toolReq, toolReq};
  OpenAIProvider provider("k");
  ToolRegistry reg = adderRegistry();
  LLMClient client(provider, conn);
  client.agentOptions().maxIterations = 2;

  Result<StructuredResult> r = client.run(sumSchema(), "loop", reg);
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::ToolIterationLimit);
}

TEST(agentloop_unknown_tool_recovers_then_answers) {
  FakeConnection conn;
  conn.responses = {
      http200(
          R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"c1","type":"function","function":{"name":"missing","arguments":"{}"}}]},"finish_reason":"tool_calls"}]})"),
      http200(R"({"choices":[{"message":{"content":"ok"},"finish_reason":"stop"}]})"),
      http200(R"({"choices":[{"message":{"content":"{\"sum\":0}"},"finish_reason":"stop"}]})"),
  };
  OpenAIProvider provider("k");
  ToolRegistry reg = adderRegistry();  // no "missing" tool
  LLMClient client(provider, conn);

  Result<StructuredResult> r = client.run(sumSchema(), "call a ghost", reg);
  CHECK(r.isOk());
  CHECK(conn.written.find("failed") != std::string::npos);  // error fed back
}

TEST(agentloop_provider_without_tools_returns_not_implemented) {
  FakeConnection conn;
  NoToolsProvider provider;
  ToolRegistry reg = adderRegistry();
  LLMClient client(provider, conn);
  Result<StructuredResult> r = client.run(sumSchema(), "hi", reg);
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::NotImplemented);
}
