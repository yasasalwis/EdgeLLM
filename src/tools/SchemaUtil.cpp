#include "SchemaUtil.h"

namespace edge {

namespace {
// Writes the schema for one field into `po` (type/description/enum, plus nested
// properties for objects and `items` for arrays). Recursion depth is bounded by
// how deep the caller built the ToolParam tree.
void writeParamSchema(const ToolParam& p, JsonObject po, bool additionalPropertiesFalse) {
  po["type"] = paramTypeName(p.type);
  if (!p.description.empty()) po["description"] = p.description;
  if (!p.enumValues.empty()) {
    JsonArray e = po["enum"].to<JsonArray>();
    for (const auto& v : p.enumValues)
      e.add(v);
  }
  if (p.type == ParamType::Object && !p.children.empty()) {
    // Fills in properties/required (and additionalProperties) alongside the
    // description already written above.
    writeObjectSchema(p.children, po, additionalPropertiesFalse);
    return;
  }
  if (p.type == ParamType::Array && p.hasItems) {
    JsonObject items = po["items"].to<JsonObject>();
    if (p.itemType == ParamType::Object && !p.children.empty()) {
      writeObjectSchema(p.children, items, additionalPropertiesFalse);
    } else {
      items["type"] = paramTypeName(p.itemType);
    }
  }
}
}  // namespace

void writeObjectSchema(const std::vector<ToolParam>& fields, JsonObject out,
                       bool additionalPropertiesFalse) {
  out["type"] = "object";
  JsonObject props = out["properties"].to<JsonObject>();
  for (const auto& p : fields) {
    JsonObject po = props[p.name].to<JsonObject>();
    writeParamSchema(p, po, additionalPropertiesFalse);
  }
  bool anyRequired = false;
  for (const auto& p : fields) {
    if (p.required) anyRequired = true;
  }
  if (anyRequired) {
    JsonArray req = out["required"].to<JsonArray>();
    for (const auto& p : fields) {
      if (p.required) req.add(p.name);
    }
  }
  // Strict objects: reject unspecified keys (required by OpenAI strict mode and
  // good hygiene). Omitted for dialects that don't accept the keyword.
  if (additionalPropertiesFalse) out["additionalProperties"] = false;
}

bool allFieldsRequired(const std::vector<ToolParam>& fields) {
  for (const auto& p : fields) {
    if (!p.required) return false;
    if (!p.children.empty() && !allFieldsRequired(p.children)) return false;
  }
  return true;
}

namespace {
bool typeMatches(ParamType type, JsonVariantConst v) {
  switch (type) {
    case ParamType::String:
      return v.is<const char*>();
    case ParamType::Integer:
      return v.is<long>();
    case ParamType::Number:
      return v.is<float>();
    case ParamType::Boolean:
      return v.is<bool>();
    case ParamType::Object:
      return v.is<JsonObjectConst>();
    case ParamType::Array:
      return v.is<JsonArrayConst>();
  }
  return false;
}

bool validateValue(const ToolParam& p, JsonVariantConst v);

// Checks every declared field of an object value (required present, values
// valid). Undeclared keys are tolerated, matching the top-level behaviour.
bool validateFields(const std::vector<ToolParam>& fields, JsonObjectConst obj) {
  for (const auto& p : fields) {
    JsonVariantConst v = obj[p.name];
    if (v.isNull()) {
      if (p.required) return false;
      continue;
    }
    if (!validateValue(p, v)) return false;
  }
  return true;
}

bool validateValue(const ToolParam& p, JsonVariantConst v) {
  if (!typeMatches(p.type, v)) return false;
  if (p.type == ParamType::String && !p.enumValues.empty()) {
    const char* s = v.as<const char*>();
    const std::string sv = s ? s : "";
    bool member = false;
    for (const auto& allowed : p.enumValues) {
      if (allowed == sv) {
        member = true;
        break;
      }
    }
    if (!member) return false;
  }
  if (p.type == ParamType::Object && !p.children.empty()) {
    return validateFields(p.children, v.as<JsonObjectConst>());
  }
  if (p.type == ParamType::Array && p.hasItems) {
    for (JsonVariantConst item : v.as<JsonArrayConst>()) {
      if (!typeMatches(p.itemType, item)) return false;
      if (p.itemType == ParamType::Object && !p.children.empty() &&
          !validateFields(p.children, item.as<JsonObjectConst>())) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

Status validateObject(const std::vector<ToolParam>& fields, const std::string& json) {
  JsonDocument doc;
  if (!json.empty()) {
    if (deserializeJson(doc, json)) return Status::fail(Error::SchemaValidationFailed);
  }
  if (!validateFields(fields, doc.as<JsonObjectConst>())) {
    return Status::fail(Error::SchemaValidationFailed);
  }
  return Status::ok();
}

}  // namespace edge
