// EdgeLLM — fluent builder for a list of schema fields, including nested ones.
// Arduino-independent. FieldSpec is the shared way to describe an object shape:
// ResponseSchema uses it for the model's answer, and ToolBuilder accepts it for
// nested tool parameters — so nested schemas read the same everywhere.
//
//   edge::FieldSpec item;
//   item.field("name", edge::ParamType::String, "product name")
//       .field("qty", edge::ParamType::Integer, "count");
//
//   edge::FieldSpec spec;
//   spec.arrayField("items", "the extracted items", item)      // array of objects
//       .arrayField("tags", "labels", edge::ParamType::String) // array of strings
//       .objectField("meta", "extra data", metaShape);         // nested object
#ifndef EDGELLM_TOOLS_FIELDSPEC_H
#define EDGELLM_TOOLS_FIELDSPEC_H

#include <string>
#include <vector>

#include "ToolTypes.h"

namespace edge {

class FieldSpec {
 public:
  FieldSpec& field(const std::string& name, ParamType type, const std::string& description,
                   bool required = true) {
    ToolParam p;
    p.name = name;
    p.type = type;
    p.description = description;
    p.required = required;
    fields_.push_back(std::move(p));
    return *this;
  }

  FieldSpec& enumField(const std::string& name, const std::string& description,
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

  // A nested object whose properties are `shape`'s fields.
  FieldSpec& objectField(const std::string& name, const std::string& description,
                         const FieldSpec& shape, bool required = true) {
    ToolParam p;
    p.name = name;
    p.type = ParamType::Object;
    p.description = description;
    p.required = required;
    p.children = shape.fields();
    fields_.push_back(std::move(p));
    return *this;
  }

  // An array of primitive elements (string/number/integer/boolean).
  FieldSpec& arrayField(const std::string& name, const std::string& description, ParamType itemType,
                        bool required = true) {
    ToolParam p;
    p.name = name;
    p.type = ParamType::Array;
    p.description = description;
    p.required = required;
    p.itemType = itemType;
    p.hasItems = true;
    fields_.push_back(std::move(p));
    return *this;
  }

  // An array of objects, each matching `itemShape`.
  FieldSpec& arrayField(const std::string& name, const std::string& description,
                        const FieldSpec& itemShape, bool required = true) {
    ToolParam p;
    p.name = name;
    p.type = ParamType::Array;
    p.description = description;
    p.required = required;
    p.itemType = ParamType::Object;
    p.hasItems = true;
    p.children = itemShape.fields();
    fields_.push_back(std::move(p));
    return *this;
  }

  const std::vector<ToolParam>& fields() const { return fields_; }
  bool empty() const { return fields_.empty(); }

 private:
  std::vector<ToolParam> fields_;
};

}  // namespace edge

#endif  // EDGELLM_TOOLS_FIELDSPEC_H
