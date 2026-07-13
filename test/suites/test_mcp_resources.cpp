// Tests for MCP resources, resource templates, and prompts.
#include <ArduinoJson.h>

#include "../../src/mcp/McpServer.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
std::string jstr(JsonVariantConst v) {
  const char* p = v.as<const char*>();
  return p ? std::string(p) : std::string();
}
JsonDocument call(McpServer& s, const std::string& body) {
  McpReply r = s.handlePost(body, true);
  JsonDocument d;
  if (!r.body.empty()) deserializeJson(d, r.body);
  return d;
}
}  // namespace

TEST(mcp_resource_list_and_read) {
  McpServer s("x", "1");
  ResourceRegistry rr;
  rr.addResource("sensor://temp", "Temperature")
      .description("room temp")
      .mimeType("text/plain")
      .onRead([](const std::string&) { return Result<std::string>::ok("22.1"); });
  s.setResourceRegistry(&rr);

  JsonDocument list = call(s, R"({"jsonrpc":"2.0","id":1,"method":"resources/list"})");
  JsonArrayConst arr = list["result"]["resources"];
  CHECK_EQ(arr.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(jstr(arr[0]["uri"]), "sensor://temp");
  CHECK_STR_EQ(jstr(arr[0]["name"]), "Temperature");

  JsonDocument read = call(
      s, R"({"jsonrpc":"2.0","id":2,"method":"resources/read","params":{"uri":"sensor://temp"}})");
  CHECK_STR_EQ(jstr(read["result"]["contents"][0]["text"]), "22.1");
  CHECK_STR_EQ(jstr(read["result"]["contents"][0]["mimeType"]), "text/plain");
}

TEST(mcp_blob_resource_served_as_blob) {
  McpServer s("x", "1");
  ResourceRegistry rr;
  rr.addResource("cam://frame", "Camera frame")
      .mimeType("image/jpeg")
      .blob()
      .onRead([](const std::string&) { return Result<std::string>::ok("AAECAwQ="); });
  s.setResourceRegistry(&rr);

  JsonDocument read = call(
      s, R"({"jsonrpc":"2.0","id":1,"method":"resources/read","params":{"uri":"cam://frame"}})");
  JsonObjectConst c = read["result"]["contents"][0];
  CHECK_STR_EQ(jstr(c["blob"]), "AAECAwQ=");
  CHECK(c["text"].isNull());  // binary resources must not use the text field
  CHECK_STR_EQ(jstr(c["mimeType"]), "image/jpeg");
}

TEST(mcp_resource_read_missing_is_32002) {
  McpServer s("x", "1");
  ResourceRegistry rr;
  s.setResourceRegistry(&rr);
  JsonDocument d = call(
      s, R"({"jsonrpc":"2.0","id":1,"method":"resources/read","params":{"uri":"sensor://ghost"}})");
  CHECK_EQ(d["error"]["code"].as<int>(), -32002);
}

TEST(mcp_resource_templates_include_kv) {
  McpServer s("x", "1");
  EdgeStore store;
  s.setStore(&store);
  JsonDocument d = call(s, R"({"jsonrpc":"2.0","id":1,"method":"resources/templates/list"})");
  CHECK_STR_EQ(jstr(d["result"]["resourceTemplates"][0]["uriTemplate"]), "kv://{key}");
}

TEST(mcp_prompts_list_and_get) {
  McpServer s("x", "1");
  PromptRegistry pr;
  pr.addPrompt("greet", "Greeting prompt")
      .arg("name", "who to greet", true)
      .onGet(
          [](ToolCallArgs& a) { return Result<std::string>::ok("Hello " + a.getString("name")); });
  s.setPromptRegistry(&pr);

  JsonDocument list = call(s, R"({"jsonrpc":"2.0","id":1,"method":"prompts/list"})");
  CHECK_STR_EQ(jstr(list["result"]["prompts"][0]["name"]), "greet");
  CHECK_STR_EQ(jstr(list["result"]["prompts"][0]["arguments"][0]["name"]), "name");
  CHECK(list["result"]["prompts"][0]["arguments"][0]["required"].as<bool>());

  JsonDocument get = call(
      s,
      R"({"jsonrpc":"2.0","id":2,"method":"prompts/get","params":{"name":"greet","arguments":{"name":"Ada"}}})");
  CHECK_STR_EQ(jstr(get["result"]["messages"][0]["role"]), "user");
  CHECK_STR_EQ(jstr(get["result"]["messages"][0]["content"]["text"]), "Hello Ada");
}

TEST(mcp_prompts_get_missing_required_arg_errors) {
  McpServer s("x", "1");
  PromptRegistry pr;
  pr.addPrompt("greet", "Greeting").arg("name", "who", true).onGet([](ToolCallArgs&) {
    return Result<std::string>::ok("hi");
  });
  s.setPromptRegistry(&pr);
  JsonDocument d = call(
      s,
      R"({"jsonrpc":"2.0","id":1,"method":"prompts/get","params":{"name":"greet","arguments":{}}})");
  CHECK_EQ(d["error"]["code"].as<int>(), -32602);
}

TEST(mcp_initialize_capabilities_reflect_registries) {
  McpServer s("x", "1");
  ToolRegistry tr;
  ResourceRegistry rr;
  PromptRegistry pr;
  s.setToolRegistry(&tr);
  s.setResourceRegistry(&rr);
  s.setPromptRegistry(&pr);
  JsonDocument d = call(s, R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
  JsonObjectConst caps = d["result"]["capabilities"];
  CHECK(!caps["tools"].isNull());
  CHECK(!caps["resources"].isNull());
  CHECK(!caps["prompts"].isNull());
}

TEST(resource_and_prompt_reregistration_replace) {
  ResourceRegistry rr;
  rr.addResource("sensor://x", "first");
  rr.addResource("sensor://x", "second");  // same URI -> redefine, not duplicate
  CHECK_EQ(rr.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(rr.find("sensor://x")->name, "second");

  PromptRegistry pr;
  pr.addPrompt("p", "first");
  pr.addPrompt("p", "second");
  CHECK_EQ(pr.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(pr.find("p")->description, "second");
}
