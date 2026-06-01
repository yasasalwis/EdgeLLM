// Tests for ResponseSchema (schema generation + validation) and StructuredResult.
#include <ArduinoJson.h>

#include "../../src/llm/ResponseSchema.h"
#include "../../src/llm/StructuredResult.h"
#include "../framework/edge_test.h"

using namespace edge;

TEST(response_schema_writes_json_schema) {
  ResponseSchema s("weather");
  s.field("temp_c", ParamType::Number, "temperature")
      .enumField("trend", "direction", {"rising", "falling", "steady"});
  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  s.writeSchema(o);

  CHECK_STR_EQ(std::string(o["type"].as<const char*>()), "object");
  CHECK_STR_EQ(std::string(o["properties"]["temp_c"]["type"].as<const char*>()), "number");
  CHECK_STR_EQ(std::string(o["properties"]["trend"]["enum"][0].as<const char*>()), "rising");
  CHECK(o["additionalProperties"].as<bool>() == false);
  CHECK_EQ(o["required"].as<JsonArrayConst>().size(), static_cast<size_t>(2));
}

TEST(response_schema_can_omit_additional_properties) {
  ResponseSchema s("x");
  s.field("a", ParamType::String, "");
  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  s.writeSchema(o, /*additionalPropertiesFalse=*/false);
  CHECK(o["additionalProperties"].isNull());
}

TEST(response_schema_validates_payloads) {
  ResponseSchema s("x");
  s.field("a", ParamType::Integer, "", true);
  s.field("b", ParamType::String, "", false);
  CHECK(s.validate(R"({"a":5})").isOk());
  CHECK(s.validate(R"({"a":5,"b":"hi"})").isOk());
  CHECK_EQ(s.validate(R"({})").error(), Error::SchemaValidationFailed);         // missing required
  CHECK_EQ(s.validate(R"({"a":"x"})").error(), Error::SchemaValidationFailed);  // wrong type
  CHECK_EQ(s.validate("not json").error(), Error::SchemaValidationFailed);
}

TEST(structured_result_typed_accessors) {
  StructuredResult r(R"({"name":"ada","age":36,"ok":true,"ratio":0.5})");
  CHECK_STR_EQ(r.getString("name"), "ada");
  CHECK_EQ(r.getInt("age"), 36L);
  CHECK(r.getBool("ok"));
  CHECK(r.getNumber("ratio") == 0.5);
  CHECK(r.has("name"));
  CHECK(!r.has("missing"));
  CHECK(r.json().find("\"name\":\"ada\"") != std::string::npos);
}
