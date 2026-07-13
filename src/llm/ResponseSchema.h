// EdgeLLM — response schema for structured output.
// Arduino-independent. Describes the JSON object the model must return. Reuses
// the same field model (ToolParam) and schema/validation utilities as tools, so
// the dialect is identical everywhere. Nested shapes (objects, arrays of
// primitives or objects) are built with FieldSpec.
//
//   edge::ResponseSchema schema("weather");
//   schema.field("temp_c", edge::ParamType::Number, "temperature in Celsius")
//         .enumField("trend", "rising/falling/steady", {"rising","falling","steady"});
//
//   edge::FieldSpec item;
//   item.field("name", edge::ParamType::String, "station name")
//       .field("temp_c", edge::ParamType::Number, "reading");
//   schema.arrayField("stations", "all stations", item);   // array of objects
#ifndef EDGELLM_LLM_RESPONSESCHEMA_H
#define EDGELLM_LLM_RESPONSESCHEMA_H

#include <string>
#include <vector>

#include "../tools/FieldSpec.h"
#include "../tools/SchemaUtil.h"
#include "../tools/ToolTypes.h"

namespace edge {

class ResponseSchema {
 public:
  explicit ResponseSchema(std::string name = "response") : name_(std::move(name)) {}

  ResponseSchema& field(const std::string& name, ParamType type, const std::string& description,
                        bool required = true) {
    spec_.field(name, type, description, required);
    return *this;
  }

  ResponseSchema& enumField(const std::string& name, const std::string& description,
                            std::vector<std::string> allowed, bool required = true) {
    spec_.enumField(name, description, std::move(allowed), required);
    return *this;
  }

  // A nested object whose properties are `shape`'s fields.
  ResponseSchema& objectField(const std::string& name, const std::string& description,
                              const FieldSpec& shape, bool required = true) {
    spec_.objectField(name, description, shape, required);
    return *this;
  }

  // An array of primitive elements (string/number/integer/boolean).
  ResponseSchema& arrayField(const std::string& name, const std::string& description,
                             ParamType itemType, bool required = true) {
    spec_.arrayField(name, description, itemType, required);
    return *this;
  }

  // An array of objects, each matching `itemShape`.
  ResponseSchema& arrayField(const std::string& name, const std::string& description,
                             const FieldSpec& itemShape, bool required = true) {
    spec_.arrayField(name, description, itemShape, required);
    return *this;
  }

  const std::string& name() const { return name_; }
  const std::vector<ToolParam>& fields() const { return spec_.fields(); }
  bool empty() const { return spec_.empty(); }

  // True if every field (recursively) is required — i.e. the schema is
  // acceptable to OpenAI's strict json_schema mode.
  bool strictCompatible() const { return allFieldsRequired(spec_.fields()); }

  // Writes the JSON Schema for this response into `out`. Pass
  // additionalPropertiesFalse=false for dialects that reject that keyword.
  void writeSchema(JsonObject out, bool additionalPropertiesFalse = true) const {
    writeObjectSchema(spec_.fields(), out, additionalPropertiesFalse);
  }

  // Validates a candidate JSON object string against this schema.
  Status validate(const std::string& json) const { return validateObject(spec_.fields(), json); }

 private:
  std::string name_;
  FieldSpec spec_;
};

}  // namespace edge

#endif  // EDGELLM_LLM_RESPONSESCHEMA_H
