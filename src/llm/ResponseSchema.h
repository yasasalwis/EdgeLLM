// EdgeLLM — response schema for structured output.
// Arduino-independent. Describes the JSON object the model must return. Reuses
// the same field model (ToolParam) and schema/validation utilities as tools, so
// the dialect is identical everywhere.
//
//   edge::ResponseSchema schema("weather");
//   schema.field("temp_c", edge::ParamType::Number, "temperature in Celsius")
//         .field("condition", edge::ParamType::String, "short description")
//         .enumField("trend", "rising/falling/steady", {"rising","falling","steady"});
#ifndef EDGELLM_LLM_RESPONSESCHEMA_H
#define EDGELLM_LLM_RESPONSESCHEMA_H

#include <string>
#include <vector>

#include "../tools/SchemaUtil.h"
#include "../tools/ToolTypes.h"

namespace edge {

class ResponseSchema {
 public:
  explicit ResponseSchema(std::string name = "response") : name_(std::move(name)) {}

  ResponseSchema& field(const std::string& name, ParamType type, const std::string& description,
                        bool required = true) {
    ToolParam p;
    p.name = name;
    p.type = type;
    p.description = description;
    p.required = required;
    fields_.push_back(std::move(p));
    return *this;
  }

  ResponseSchema& enumField(const std::string& name, const std::string& description,
                            std::vector<std::string> allowed, bool required = true) {
    ToolParam p;
    p.name = name;
    p.type = ParamType::String;
    p.description = description;
    p.required = required;
    p.enumValues = std::move(allowed);
    fields_.push_back(std::move(p));
    return *this;
  }

  const std::string& name() const { return name_; }
  const std::vector<ToolParam>& fields() const { return fields_; }
  bool empty() const { return fields_.empty(); }

  // Writes the JSON Schema for this response into `out`. Pass
  // additionalPropertiesFalse=false for dialects that reject that keyword.
  void writeSchema(JsonObject out, bool additionalPropertiesFalse = true) const {
    writeObjectSchema(fields_, out, additionalPropertiesFalse);
  }

  // Validates a candidate JSON object string against this schema.
  Status validate(const std::string& json) const { return validateObject(fields_, json); }

 private:
  std::string name_;
  std::vector<ToolParam> fields_;
};

}  // namespace edge

#endif  // EDGELLM_LLM_RESPONSESCHEMA_H
