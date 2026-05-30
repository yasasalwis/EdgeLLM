// End-to-end agent-loop tests: the model requests a tool, the loop runs it,
// feeds the result back, and the model produces a final answer. Driven by a
// scripted multi-response fake socket.
#include <string>

#include "../../src/llm/LLMClient.h"
#include "../../src/llm/providers/AnthropicProvider.h"
#include "../../src/llm/providers/GeminiProvider.h"
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
}  // namespace

TEST(agentloop_openai_runs_tool_then_answers) {
  FakeConnection conn;
  conn.responses = {
      http200(
          R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"call_1","type":"function","function":{"name":"add","arguments":"{\"a\":2,\"b\":3}"}}]},"finish_reason":"tool_calls"}]})"),
      http200(R"({"choices":[{"message":{"content":"The sum is 5"},"finish_reason":"stop"}]})"),
  };
  OpenAIProvider provider("k");
  ToolRegistry reg = adderRegistry();
  LLMClient client(provider, conn);

  Result<ChatResult> r = client.run("add 2 and 3", reg);
  CHECK(r.isOk());
  CHECK_STR_EQ(r.value().text, "The sum is 5");
  // The second request carried the tool result back to the model.
  CHECK(conn.written.find("\"tool_call_id\":\"call_1\"") != std::string::npos);
  CHECK(conn.written.find("\"content\":\"5\"") != std::string::npos);
}

TEST(agentloop_anthropic_runs_tool_then_answers) {
  FakeConnection conn;
  conn.responses = {
      http200(
          R"({"content":[{"type":"tool_use","id":"toolu_1","name":"add","input":{"a":4,"b":5}}],"stop_reason":"tool_use"})"),
      http200(R"({"content":[{"type":"text","text":"It is 9"}],"stop_reason":"end_turn"})"),
  };
  AnthropicProvider provider("k");
  ToolRegistry reg = adderRegistry();
  LLMClient client(provider, conn);

  Result<ChatResult> r = client.run("add 4 and 5", reg);
  CHECK(r.isOk());
  CHECK_STR_EQ(r.value().text, "It is 9");
  CHECK(conn.written.find("\"tool_use_id\":\"toolu_1\"") != std::string::npos);
  CHECK(conn.written.find("\"content\":\"9\"") != std::string::npos);
}

TEST(agentloop_enforces_max_iterations) {
  FakeConnection conn;
  // Every response keeps asking for the tool — the model never settles.
  std::string toolReq = http200(
      R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"c","type":"function","function":{"name":"add","arguments":"{\"a\":1,\"b\":1}"}}]},"finish_reason":"tool_calls"}]})");
  conn.responses = {toolReq, toolReq, toolReq, toolReq};
  OpenAIProvider provider("k");
  ToolRegistry reg = adderRegistry();
  LLMClient client(provider, conn);
  client.agentOptions().maxIterations = 2;

  Result<ChatResult> r = client.run("loop forever", reg);
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::ToolIterationLimit);
}

TEST(agentloop_unknown_tool_is_reported_to_model) {
  FakeConnection conn;
  conn.responses = {
      http200(
          R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"c1","type":"function","function":{"name":"missing","arguments":"{}"}}]},"finish_reason":"tool_calls"}]})"),
      http200(R"({"choices":[{"message":{"content":"Sorry, I cannot."},"finish_reason":"stop"}]})"),
  };
  OpenAIProvider provider("k");
  ToolRegistry reg = adderRegistry();  // has no "missing" tool
  LLMClient client(provider, conn);

  Result<ChatResult> r = client.run("call a ghost", reg);
  CHECK(r.isOk());  // loop recovers; error is fed back, model answers
  CHECK_STR_EQ(r.value().text, "Sorry, I cannot.");
  CHECK(conn.written.find("failed") != std::string::npos);  // error result sent back
}

TEST(agentloop_provider_without_tools_returns_not_implemented) {
  FakeConnection conn;
  GeminiProvider provider("k");  // tool calling not yet implemented for Gemini
  ToolRegistry reg = adderRegistry();
  LLMClient client(provider, conn);
  Result<ChatResult> r = client.run("hi", reg);
  CHECK(!r.isOk());
  CHECK_EQ(r.error(), Error::NotImplemented);
}
