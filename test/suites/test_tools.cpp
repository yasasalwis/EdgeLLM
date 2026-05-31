// Tests for the shared ToolRegistry: fluent registration, JSON-Schema build,
// argument validation, and dispatch.
#include <ArduinoJson.h>

#include "../../src/tools/ToolRegistry.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
ToolRegistry makeRegistry() {
  ToolRegistry r;
  r.addTool("set_led", "Turn an LED on or off")
      .param("pin", ParamType::Integer, "GPIO pin", true)
      .paramEnum("state", "on or off", {"on", "off"}, true)
      .mutating()
      .allowWrite()
      .onCall([](ToolCallArgs& a) {
        return ToolResult::ok(std::string("led ") + a.getString("state") + " on pin " +
                              std::to_string(a.getInt("pin")));
      });
  return r;
}
}  // namespace

TEST(tool_registry_registers_and_finds) {
  ToolRegistry r = makeRegistry();
  CHECK_EQ(r.size(), static_cast<size_t>(1));
  CHECK(r.has("set_led"));
  CHECK(!r.has("nope"));
  const Tool* t = r.find("set_led");
  CHECK(t != nullptr);
  CHECK(t->mutating);
  CHECK(t->writeAllowed);
  CHECK_EQ(t->params.size(), static_cast<size_t>(2));
}

TEST(tool_schema_is_valid_json_schema) {
  ToolRegistry r = makeRegistry();
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  writeToolSchema(*r.find("set_led"), root);

  CHECK_STR_EQ(std::string(root["type"].as<const char*>()), "object");
  CHECK_STR_EQ(std::string(root["properties"]["pin"]["type"].as<const char*>()), "integer");
  CHECK_STR_EQ(std::string(root["properties"]["state"]["type"].as<const char*>()), "string");
  // enum present for state
  CHECK_STR_EQ(std::string(root["properties"]["state"]["enum"][0].as<const char*>()), "on");
  // both params required
  CHECK_EQ(root["required"].as<JsonArrayConst>().size(), static_cast<size_t>(2));
}

TEST(tool_validate_accepts_good_args) {
  ToolRegistry r = makeRegistry();
  CHECK(r.validate(*r.find("set_led"), R"({"pin":13,"state":"on"})").isOk());
}

TEST(tool_validate_rejects_missing_required) {
  ToolRegistry r = makeRegistry();
  Status s = r.validate(*r.find("set_led"), R"({"pin":13})");
  CHECK(!s.isOk());
  CHECK_EQ(s.error(), Error::SchemaValidationFailed);
}

TEST(tool_validate_rejects_wrong_type) {
  ToolRegistry r = makeRegistry();
  // pin should be integer, not string
  CHECK_EQ(r.validate(*r.find("set_led"), R"({"pin":"thirteen","state":"on"})").error(),
           Error::SchemaValidationFailed);
}

TEST(tool_validate_rejects_bad_enum) {
  ToolRegistry r = makeRegistry();
  CHECK_EQ(r.validate(*r.find("set_led"), R"({"pin":13,"state":"blink"})").error(),
           Error::SchemaValidationFailed);
}

TEST(tool_dispatch_runs_handler) {
  ToolRegistry r = makeRegistry();
  Result<ToolResult> res = r.dispatch("set_led", R"({"pin":13,"state":"on"})");
  CHECK(res.isOk());
  CHECK(!res.value().isError);
  CHECK_STR_EQ(res.value().content, "led on on pin 13");
}

TEST(tool_dispatch_unknown_tool_fails) {
  ToolRegistry r = makeRegistry();
  Result<ToolResult> res = r.dispatch("ghost", "{}");
  CHECK(!res.isOk());
  CHECK_EQ(res.error(), Error::NotFound);
}

TEST(tool_dispatch_bad_args_returns_error_result) {
  ToolRegistry r = makeRegistry();
  // Invalid args -> dispatch succeeds but the ToolResult is an error (so the
  // agent loop can feed it back to the model rather than aborting).
  Result<ToolResult> res = r.dispatch("set_led", R"({"state":"on"})");
  CHECK(res.isOk());
  CHECK(res.value().isError);
}

TEST(tool_optional_param_can_be_omitted) {
  ToolRegistry r;
  r.addTool("greet", "greet someone")
      .param("name", ParamType::String, "who", false)
      .onCall([](ToolCallArgs& a) { return ToolResult::ok("hi " + a.getString("name", "world")); });
  Result<ToolResult> res = r.dispatch("greet", "{}");
  CHECK(res.isOk());
  CHECK_STR_EQ(res.value().content, "hi world");
}

TEST(tool_reregistration_replaces_not_duplicates) {
  ToolRegistry r;
  r.addTool("x", "first").onCall([](ToolCallArgs&) { return ToolResult::ok("a"); });
  r.addTool("x", "second").onCall([](ToolCallArgs&) { return ToolResult::ok("b"); });
  CHECK_EQ(r.size(), static_cast<size_t>(1));  // no duplicate
  CHECK_STR_EQ(r.find("x")->description, "second");
  CHECK_STR_EQ(r.dispatch("x", "{}").value().content, "b");  // latest handler wins
}
