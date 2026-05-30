// Tests for tool-calling request building and response parsing on the two
// providers that support it in Phase 3: OpenAI and Anthropic.
#include <ArduinoJson.h>

#include "../../src/llm/providers/AnthropicProvider.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../../src/tools/ToolRegistry.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
ToolRegistry calculatorRegistry() {
  ToolRegistry r;
  r.addTool("add", "Add two numbers")
      .param("a", ParamType::Number, "first", true)
      .param("b", ParamType::Number, "second", true)
      .onCall([](ToolCallArgs& args) {
        return ToolResult::ok(std::to_string(args.getNumber("a") + args.getNumber("b")));
      });
  return r;
}

JsonDocument parse(const std::string& s) {
  JsonDocument d;
  deserializeJson(d, s);
  return d;
}

// Builds a small history: user asked, assistant called the tool, tool answered.
MessageList toolHistory() {
  MessageList m{Message::user("add 2 and 3")};
  ToolCall call;
  call.id = "call_1";
  call.name = "add";
  call.argumentsJson = R"({"a":2,"b":3})";
  Message assistant(Role::Assistant, "");
  assistant.toolCalls.push_back(call);
  m.push_back(assistant);
  m.push_back(Message::toolResult(call, ToolResult::ok("5")));
  return m;
}
}  // namespace

// ----------------------------- OpenAI -----------------------------

TEST(openai_supports_tools) {
  OpenAIProvider p("k");
  CHECK(p.supportsTools());
}

TEST(openai_build_tool_request_advertises_tools) {
  OpenAIProvider p("k", "gpt-x");
  ToolRegistry reg = calculatorRegistry();
  MessageList m{Message::user("add 2 and 3")};
  HttpRequest req;
  CHECK(p.buildToolRequest(m, ChatOptions{}, reg, req).isOk());

  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(std::string(d["tools"][0]["type"].as<const char*>()), "function");
  CHECK_STR_EQ(std::string(d["tools"][0]["function"]["name"].as<const char*>()), "add");
  CHECK_STR_EQ(
      std::string(d["tools"][0]["function"]["parameters"]["type"].as<const char*>()), "object");
}

TEST(openai_serializes_tool_turn_and_result) {
  OpenAIProvider p("k");
  ToolRegistry reg = calculatorRegistry();
  HttpRequest req;
  p.buildToolRequest(toolHistory(), ChatOptions{}, reg, req);
  JsonDocument d = parse(req.body);

  // messages: [user, assistant(tool_calls), tool]
  JsonArrayConst msgs = d["messages"];
  CHECK_EQ(msgs.size(), static_cast<size_t>(3));
  CHECK_STR_EQ(std::string(d["messages"][1]["role"].as<const char*>()), "assistant");
  CHECK_STR_EQ(
      std::string(d["messages"][1]["tool_calls"][0]["function"]["name"].as<const char*>()), "add");
  CHECK_STR_EQ(std::string(d["messages"][2]["role"].as<const char*>()), "tool");
  CHECK_STR_EQ(std::string(d["messages"][2]["tool_call_id"].as<const char*>()), "call_1");
  CHECK_STR_EQ(std::string(d["messages"][2]["content"].as<const char*>()), "5");
}

TEST(openai_parses_tool_call_response) {
  OpenAIProvider p("k");
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"call_1","type":"function","function":{"name":"add","arguments":"{\"a\":2,\"b\":3}"}}]},"finish_reason":"tool_calls"}]})";
  AgentTurn turn;
  CHECK(p.parseToolResponse(resp, turn).isOk());
  CHECK(turn.wantsTools());
  CHECK_EQ(turn.toolCalls.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(turn.toolCalls[0].id, "call_1");
  CHECK_STR_EQ(turn.toolCalls[0].name, "add");
  CHECK(turn.toolCalls[0].argumentsJson.find("\"a\":2") != std::string::npos);
}

// ----------------------------- Anthropic -----------------------------

TEST(anthropic_supports_tools) {
  AnthropicProvider p("k");
  CHECK(p.supportsTools());
}

TEST(anthropic_build_tool_request_advertises_tools) {
  AnthropicProvider p("k", "claude-x");
  ToolRegistry reg = calculatorRegistry();
  MessageList m{Message::user("add 2 and 3")};
  HttpRequest req;
  CHECK(p.buildToolRequest(m, ChatOptions{}, reg, req).isOk());
  JsonDocument d = parse(req.body);
  CHECK_STR_EQ(std::string(d["tools"][0]["name"].as<const char*>()), "add");
  CHECK_STR_EQ(std::string(d["tools"][0]["input_schema"]["type"].as<const char*>()), "object");
}

TEST(anthropic_serializes_tool_use_and_result_blocks) {
  AnthropicProvider p("k");
  ToolRegistry reg = calculatorRegistry();
  HttpRequest req;
  p.buildToolRequest(toolHistory(), ChatOptions{}, reg, req);
  JsonDocument d = parse(req.body);

  // messages: [user, assistant(tool_use block), user(tool_result block)]
  CHECK_EQ(d["messages"].as<JsonArrayConst>().size(), static_cast<size_t>(3));
  CHECK_STR_EQ(std::string(d["messages"][1]["role"].as<const char*>()), "assistant");
  CHECK_STR_EQ(std::string(d["messages"][1]["content"][0]["type"].as<const char*>()), "tool_use");
  CHECK_STR_EQ(std::string(d["messages"][1]["content"][0]["name"].as<const char*>()), "add");
  // input parsed back into an object (not a string)
  CHECK_EQ(d["messages"][1]["content"][0]["input"]["a"].as<int>(), 2);

  CHECK_STR_EQ(std::string(d["messages"][2]["role"].as<const char*>()), "user");
  CHECK_STR_EQ(std::string(d["messages"][2]["content"][0]["type"].as<const char*>()),
              "tool_result");
  CHECK_STR_EQ(std::string(d["messages"][2]["content"][0]["tool_use_id"].as<const char*>()),
              "call_1");
}

TEST(anthropic_parses_tool_use_response) {
  AnthropicProvider p("k");
  HttpResponse resp;
  resp.status = 200;
  resp.body =
      R"({"content":[{"type":"text","text":"Let me add."},{"type":"tool_use","id":"toolu_1","name":"add","input":{"a":2,"b":3}}],"stop_reason":"tool_use"})";
  AgentTurn turn;
  CHECK(p.parseToolResponse(resp, turn).isOk());
  CHECK_STR_EQ(turn.text, "Let me add.");
  CHECK_EQ(turn.toolCalls.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(turn.toolCalls[0].id, "toolu_1");
  CHECK_STR_EQ(turn.toolCalls[0].name, "add");
  CHECK(turn.toolCalls[0].argumentsJson.find("\"a\":2") != std::string::npos);
}
