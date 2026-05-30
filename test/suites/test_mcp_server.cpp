// Tests for the MCP server protocol core: initialize, ping, notifications,
// tools (incl. deny-by-default writes), batches, auth, errors, and the built-in
// EdgeStore exposure.
#include <ArduinoJson.h>

#include "../../src/mcp/McpServer.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
std::string jstr(JsonVariantConst v) {
  const char* p = v.as<const char*>();
  return p ? std::string(p) : std::string();
}

JsonDocument call(McpServer& s, const std::string& body, bool authorized = true,
                  McpReply* outReply = nullptr) {
  McpReply r = s.handlePost(body, authorized);
  if (outReply) *outReply = r;
  JsonDocument d;
  if (!r.body.empty()) deserializeJson(d, r.body);
  return d;
}

ToolRegistry sampleTools() {
  ToolRegistry reg;
  reg.addTool("read_temp", "Read temperature").onCall([](ToolCallArgs&) {
    return ToolResult::ok("21.5");
  });
  reg.addTool("danger", "Disabled write").mutating().onCall([](ToolCallArgs&) {
    return ToolResult::ok("should not run");
  });
  reg.addTool("set_led", "Allowed write")
      .paramEnum("state", "on/off", {"on", "off"}, true)
      .mutating()
      .allowWrite()
      .onCall([](ToolCallArgs& a) { return ToolResult::ok("led " + a.getString("state")); });
  reg.addTool("echo", "Echo a message")
      .param("msg", ParamType::String, "text", true)
      .onCall([](ToolCallArgs& a) { return ToolResult::ok("echoed:" + a.getString("msg")); });
  return reg;
}
}  // namespace

TEST(mcp_initialize_returns_server_info_and_session) {
  McpServer s("EdgeLLM-Test", "9.9");
  s.setInstructions("be careful");
  McpReply reply;
  JsonDocument d = call(
      s, R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18"}})",
      true, &reply);
  CHECK_STR_EQ(jstr(d["jsonrpc"]), "2.0");
  CHECK_EQ(d["id"].as<int>(), 1);
  CHECK_STR_EQ(jstr(d["result"]["protocolVersion"]), "2025-06-18");
  CHECK_STR_EQ(jstr(d["result"]["serverInfo"]["name"]), "EdgeLLM-Test");
  CHECK_STR_EQ(jstr(d["result"]["serverInfo"]["version"]), "9.9");
  CHECK_STR_EQ(jstr(d["result"]["instructions"]), "be careful");
  CHECK(reply.setSession);
  CHECK(!reply.sessionId.empty());
}

TEST(mcp_notification_yields_202_no_body) {
  McpServer s("x", "1");
  McpReply r = s.handlePost(R"({"jsonrpc":"2.0","method":"notifications/initialized"})", true);
  CHECK(r.isNotificationOnly);
  CHECK_EQ(r.httpStatus, 202);
  CHECK(r.body.empty());
}

TEST(mcp_ping_returns_empty_result) {
  McpServer s("x", "1");
  JsonDocument d = call(s, R"({"jsonrpc":"2.0","id":2,"method":"ping"})");
  CHECK(!d["result"].isNull());
  CHECK(d["error"].isNull());
}

TEST(mcp_tools_list_hides_denied_writes) {
  McpServer s("x", "1");
  ToolRegistry reg = sampleTools();
  s.setToolRegistry(&reg);
  JsonDocument d = call(s, R"({"jsonrpc":"2.0","id":3,"method":"tools/list"})");
  JsonArrayConst tools = d["result"]["tools"];
  // read_temp, set_led, echo are exposed; "danger" (mutating, not allowed) is not.
  CHECK_EQ(tools.size(), static_cast<size_t>(3));
  bool sawDanger = false, sawReadTemp = false;
  bool readTempReadOnly = false;
  for (JsonObjectConst t : tools) {
    const std::string name = jstr(t["name"]);
    if (name == "danger") sawDanger = true;
    if (name == "read_temp") {
      sawReadTemp = true;
      readTempReadOnly = t["annotations"]["readOnlyHint"].as<bool>();
    }
  }
  CHECK(sawReadTemp);
  CHECK(!sawDanger);
  CHECK(readTempReadOnly);
}

TEST(mcp_tools_call_runs_tool) {
  McpServer s("x", "1");
  ToolRegistry reg = sampleTools();
  s.setToolRegistry(&reg);
  JsonDocument d = call(
      s, R"({"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"echo","arguments":{"msg":"hi"}}})");
  CHECK_STR_EQ(jstr(d["result"]["content"][0]["text"]), "echoed:hi");
  CHECK(!d["result"]["isError"].as<bool>());
}

TEST(mcp_tools_call_refuses_denied_write) {
  McpServer s("x", "1");
  ToolRegistry reg = sampleTools();
  s.setToolRegistry(&reg);
  JsonDocument d = call(
      s, R"({"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"danger","arguments":{}}})");
  CHECK_EQ(d["error"]["code"].as<int>(), -32602);
}

TEST(mcp_tools_call_unknown_tool_errors) {
  McpServer s("x", "1");
  ToolRegistry reg = sampleTools();
  s.setToolRegistry(&reg);
  JsonDocument d = call(
      s, R"({"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"ghost"}})");
  CHECK_EQ(d["error"]["code"].as<int>(), -32602);
}

TEST(mcp_unknown_method_errors) {
  McpServer s("x", "1");
  JsonDocument d = call(s, R"({"jsonrpc":"2.0","id":7,"method":"foo/bar"})");
  CHECK_EQ(d["error"]["code"].as<int>(), -32601);
}

TEST(mcp_parse_error_is_400) {
  McpServer s("x", "1");
  McpReply r = s.handlePost("{ not valid json", true);
  CHECK_EQ(r.httpStatus, 400);
  JsonDocument d;
  deserializeJson(d, r.body);
  CHECK_EQ(d["error"]["code"].as<int>(), -32700);
}

TEST(mcp_auth_required_blocks_unauthorized) {
  McpServer s("x", "1");
  s.setAuthToken("secret");
  CHECK(s.authRequired());
  CHECK(s.checkAuth("Bearer secret"));
  CHECK(!s.checkAuth("Bearer wrong"));
  CHECK(!s.checkAuth(""));

  McpReply blocked = s.handlePost(R"({"jsonrpc":"2.0","id":1,"method":"ping"})", false);
  CHECK_EQ(blocked.httpStatus, 401);
  McpReply ok = s.handlePost(R"({"jsonrpc":"2.0","id":1,"method":"ping"})", true);
  CHECK_EQ(ok.httpStatus, 200);
}

TEST(mcp_batch_skips_notifications) {
  McpServer s("x", "1");
  JsonDocument d = call(
      s,
      R"([{"jsonrpc":"2.0","id":1,"method":"ping"},{"jsonrpc":"2.0","method":"notifications/initialized"}])");
  CHECK(d.is<JsonArray>());
  CHECK_EQ(d.as<JsonArrayConst>().size(), static_cast<size_t>(1));
  CHECK_EQ(d[0]["id"].as<int>(), 1);
}

TEST(mcp_edgestore_exposed_as_tools_and_resources) {
  McpServer s("x", "1");
  EdgeStore store;
  s.setStore(&store, /*allowWrites=*/true);

  // kv_set stores a value.
  JsonDocument set = call(
      s,
      R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"kv_set","arguments":{"key":"mode","value":"eco"}}})");
  CHECK(!set["result"]["isError"].as<bool>());
  CHECK_STR_EQ(store.get("mode").value(), "eco");

  // tools/list includes kv_set and kv_delete.
  JsonDocument list = call(s, R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");
  bool sawKvSet = false, sawKvDelete = false;
  for (JsonObjectConst t : list["result"]["tools"].as<JsonArrayConst>()) {
    if (jstr(t["name"]) == "kv_set") sawKvSet = true;
    if (jstr(t["name"]) == "kv_delete") sawKvDelete = true;
  }
  CHECK(sawKvSet);
  CHECK(sawKvDelete);

  // resources/list includes kv://mode, and resources/read returns its value.
  JsonDocument res = call(s, R"({"jsonrpc":"2.0","id":3,"method":"resources/list"})");
  bool sawKvResource = false;
  for (JsonObjectConst r : res["result"]["resources"].as<JsonArrayConst>()) {
    if (jstr(r["uri"]) == "kv://mode") sawKvResource = true;
  }
  CHECK(sawKvResource);

  JsonDocument read = call(
      s, R"({"jsonrpc":"2.0","id":4,"method":"resources/read","params":{"uri":"kv://mode"}})");
  CHECK_STR_EQ(jstr(read["result"]["contents"][0]["text"]), "eco");
}

TEST(mcp_edgestore_writes_denied_by_default) {
  McpServer s("x", "1");
  EdgeStore store;
  s.setStore(&store);  // allowWrites defaults to false
  // kv_set is not exposed and not callable.
  JsonDocument list = call(s, R"({"jsonrpc":"2.0","id":1,"method":"tools/list"})");
  CHECK_EQ(list["result"]["tools"].as<JsonArrayConst>().size(), static_cast<size_t>(0));
  JsonDocument call_ = call(
      s,
      R"({"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"kv_set","arguments":{"key":"k","value":"v"}}})");
  CHECK_EQ(call_["error"]["code"].as<int>(), -32602);
}
