#include "SchemaUtil.h"

namespace edge {

void writeObjectSchema(const std::vector<ToolParam>& fields, JsonObject out,
                       bool additionalPropertiesFalse) {
  out["type"] = "object";
  JsonObject props = out["properties"].to<JsonObject>();
  for (const auto& p : fields) {
    JsonObject po = props[p.name].to<JsonObject>();
    po["type"] = paramTypeName(p.type);
    if (!p.description.empty()) po["description"] = p.description;
    if (!p.enumValues.empty()) {
      JsonArray e = po["enum"].to<JsonArray>();
      for (const auto& v : p.enumValues)
        e.add(v);
    }
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
}  // namespace

Status validateObject(const std::vector<ToolParam>& fields, const std::string& json) {
  JsonDocument doc;
  if (!json.empty()) {
    if (deserializeJson(doc, json)) return Status::fail(Error::SchemaValidationFailed);
  }
  for (const auto& p : fields) {
    JsonVariantConst v = doc[p.name];
    if (v.isNull()) {
      if (p.required) return Status::fail(Error::SchemaValidationFailed);
      continue;
    }
    if (!typeMatches(p.type, v)) return Status::fail(Error::SchemaValidationFailed);
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
      if (!member) return Status::fail(Error::SchemaValidationFailed);
    }
  }
  return Status::ok();
}

}  // namespace edge
