// Tests for nested schemas: objects with properties, arrays with typed items
// (including arrays of objects), recursive validation, and the OpenAI
// strict-mode compatibility rule.
#include <ArduinoJson.h>

#include "../../src/llm/ResponseSchema.h"
#include "../../src/llm/providers/OpenAIChatProvider.h"
#include "../../src/tools/FieldSpec.h"
#include "../../src/tools/ToolRegistry.h"
#include "../framework/edge_test.h"

using namespace edge;

TEST(schema_array_of_primitives_writes_items) {
  ResponseSchema s("tags");
  s.arrayField("tags", "labels", ParamType::String);
  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  s.writeSchema(o);
  CHECK_STR_EQ(std::string(o["properties"]["tags"]["type"].as<const char*>()), "array");
  CHECK_STR_EQ(std::string(o["properties"]["tags"]["items"]["type"].as<const char*>()), "string");
}

TEST(schema_array_of_objects_writes_item_schema) {
  FieldSpec item;
  item.field("name", ParamType::String, "product name").field("qty", ParamType::Integer, "count");
  ResponseSchema s("extraction");
  s.arrayField("items", "extracted items", item);

  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  s.writeSchema(o);
  JsonObject items = o["properties"]["items"]["items"];
  CHECK_STR_EQ(std::string(items["type"].as<const char*>()), "object");
  CHECK_STR_EQ(std::string(items["properties"]["name"]["type"].as<const char*>()), "string");
  CHECK_STR_EQ(std::string(items["properties"]["qty"]["type"].as<const char*>()), "integer");
  CHECK_EQ(items["required"].as<JsonArrayConst>().size(), static_cast<size_t>(2));
  CHECK(items["additionalProperties"].as<bool>() == false);
}

TEST(schema_nested_object_writes_properties) {
  FieldSpec address;
  address.field("city", ParamType::String, "city name")
      .field("zip", ParamType::String, "postal code", false);
  ResponseSchema s("person");
  s.field("name", ParamType::String, "full name").objectField("address", "where", address);

  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  s.writeSchema(o);
  JsonObject addr = o["properties"]["address"];
  CHECK_STR_EQ(std::string(addr["type"].as<const char*>()), "object");
  CHECK_STR_EQ(std::string(addr["properties"]["city"]["type"].as<const char*>()), "string");
  // Only the required child appears in the nested required list.
  CHECK_EQ(addr["required"].as<JsonArrayConst>().size(), static_cast<size_t>(1));
  CHECK_STR_EQ(std::string(addr["description"].as<const char*>()), "where");
}

TEST(schema_nested_omits_additional_properties_for_gemini_dialect) {
  FieldSpec item;
  item.field("a", ParamType::String, "");
  ResponseSchema s("x");
  s.arrayField("list", "", item).objectField("obj", "", item);

  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  s.writeSchema(o, /*additionalPropertiesFalse=*/false);
  CHECK(o["additionalProperties"].isNull());
  CHECK(o["properties"]["list"]["items"]["additionalProperties"].isNull());
  CHECK(o["properties"]["obj"]["additionalProperties"].isNull());
}

TEST(schema_validates_array_items_recursively) {
  FieldSpec item;
  item.field("name", ParamType::String, "").field("qty", ParamType::Integer, "");
  ResponseSchema s("x");
  s.arrayField("items", "", item);

  CHECK(s.validate(R"({"items":[{"name":"bolt","qty":4},{"name":"nut","qty":9}]})").isOk());
  CHECK(s.validate(R"({"items":[]})").isOk());
  // Element missing a required field.
  CHECK_EQ(s.validate(R"({"items":[{"name":"bolt"}]})").error(), Error::SchemaValidationFailed);
  // Element field has the wrong type.
  CHECK_EQ(s.validate(R"({"items":[{"name":"bolt","qty":"four"}]})").error(),
           Error::SchemaValidationFailed);
  // Element is not an object at all.
  CHECK_EQ(s.validate(R"({"items":["bolt"]})").error(), Error::SchemaValidationFailed);
}

TEST(schema_validates_primitive_array_items) {
  ResponseSchema s("x");
  s.arrayField("nums", "", ParamType::Integer);
  CHECK(s.validate(R"({"nums":[1,2,3]})").isOk());
  CHECK_EQ(s.validate(R"({"nums":[1,"two"]})").error(), Error::SchemaValidationFailed);
}

TEST(schema_validates_nested_object_and_enum) {
  FieldSpec inner;
  inner.enumField("mode", "", {"on", "off"});
  ResponseSchema s("x");
  s.objectField("config", "", inner);
  CHECK(s.validate(R"({"config":{"mode":"on"}})").isOk());
  CHECK_EQ(s.validate(R"({"config":{"mode":"blink"}})").error(), Error::SchemaValidationFailed);
  CHECK_EQ(s.validate(R"({"config":{}})").error(), Error::SchemaValidationFailed);
  CHECK_EQ(s.validate(R"({"config":"on"})").error(), Error::SchemaValidationFailed);
}

TEST(schema_optional_nested_object_may_be_absent) {
  FieldSpec inner;
  inner.field("a", ParamType::String, "");
  ResponseSchema s("x");
  s.field("id", ParamType::Integer, "").objectField("extra", "", inner, /*required=*/false);
  CHECK(s.validate(R"({"id":1})").isOk());
  CHECK(s.validate(R"({"id":1,"extra":{"a":"hi"}})").isOk());
  CHECK_EQ(s.validate(R"({"id":1,"extra":{}})").error(), Error::SchemaValidationFailed);
}

TEST(schema_strict_compatibility_recurses) {
  FieldSpec allReq;
  allReq.field("a", ParamType::String, "");
  ResponseSchema strict("x");
  strict.field("id", ParamType::Integer, "").arrayField("list", "", allReq);
  CHECK(strict.strictCompatible());

  FieldSpec withOptional;
  withOptional.field("a", ParamType::String, "", /*required=*/false);
  ResponseSchema loose("y");
  loose.field("id", ParamType::Integer, "").objectField("obj", "", withOptional);
  CHECK(!loose.strictCompatible());
}

TEST(openai_downgrades_strict_for_optional_fields) {
  OpenAIProvider p("key");
  MessageList msgs{Message::user("hi")};
  ChatOptions opts;

  // All-required schema -> strict mode.
  ResponseSchema strict("s");
  strict.field("a", ParamType::String, "");
  HttpRequest req1;
  CHECK(p.buildStructuredRequest(msgs, opts, strict, req1).isOk());
  JsonDocument d1;
  CHECK(deserializeJson(d1, req1.body) == DeserializationError::Ok);
  CHECK(d1["response_format"]["json_schema"]["strict"].as<bool>() == true);

  // Optional field -> non-strict (strict mode would reject the request).
  ResponseSchema loose("s");
  loose.field("a", ParamType::String, "").field("b", ParamType::String, "", false);
  HttpRequest req2;
  CHECK(p.buildStructuredRequest(msgs, opts, loose, req2).isOk());
  JsonDocument d2;
  CHECK(deserializeJson(d2, req2.body) == DeserializationError::Ok);
  CHECK(d2["response_format"]["json_schema"]["strict"].as<bool>() == false);
}

TEST(tool_builder_supports_nested_params) {
  ToolRegistry reg;
  FieldSpec point;
  point.field("x", ParamType::Number, "").field("y", ParamType::Number, "");
  reg.addTool("draw_path", "Draw a path")
      .paramArray("points", "the path", point)
      .paramArray("labels", "names", ParamType::String, false)
      .paramObject("style", "stroke config", point, false)
      .onCall([](ToolCallArgs&) { return ToolResult::ok("drawn"); });

  const Tool* t = reg.find("draw_path");
  CHECK(t != nullptr);
  JsonDocument d;
  JsonObject schema = d.to<JsonObject>();
  writeToolSchema(*t, schema);
  CHECK_STR_EQ(std::string(schema["properties"]["points"]["items"]["type"].as<const char*>()),
               "object");
  CHECK_STR_EQ(std::string(schema["properties"]["labels"]["items"]["type"].as<const char*>()),
               "string");

  // Dispatch validates nested arguments before running the handler.
  auto ok = reg.dispatch("draw_path", R"({"points":[{"x":1.5,"y":2.0}]})");
  CHECK(ok.isOk());
  CHECK(!ok.value().isError);
  auto bad = reg.dispatch("draw_path", R"({"points":[{"x":1.5}]})");
  CHECK(bad.isOk());
  CHECK(bad.value().isError);  // fed back to the model as a tool error
}
