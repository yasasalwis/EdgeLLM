#include "ToolRegistry.h"

#include <ArduinoJson.h>

namespace edge {

// ---------------- Schema writer ----------------

void writeToolSchema(const Tool& tool, JsonObject schemaOut) {
  schemaOut["type"] = "object";
  JsonObject props = schemaOut["properties"].to<JsonObject>();
  bool anyRequired = false;
  for (const auto& p : tool.params) {
    if (p.required) anyRequired = true;
  }
  for (const auto& p : tool.params) {
    JsonObject po = props[p.name].to<JsonObject>();
    po["type"] = paramTypeName(p.type);
    if (!p.description.empty()) po["description"] = p.description;
    if (!p.enumValues.empty()) {
      JsonArray e = po["enum"].to<JsonArray>();
      for (const auto& v : p.enumValues) e.add(v);
    }
  }
  if (anyRequired) {
    JsonArray req = schemaOut["required"].to<JsonArray>();
    for (const auto& p : tool.params) {
      if (p.required) req.add(p.name);
    }
  }
}

// ---------------- Builder ----------------

Tool& ToolBuilder::tool() { return registry_->tools_[index_]; }

ToolBuilder& ToolBuilder::param(const std::string& name, ParamType type,
                                const std::string& description, bool required) {
  ToolParam p;
  p.name = name;
  p.type = type;
  p.description = description;
  p.required = required;
  tool().params.push_back(std::move(p));
  return *this;
}

ToolBuilder& ToolBuilder::paramEnum(const std::string& name, const std::string& description,
                                    std::vector<std::string> allowed, bool required) {
  ToolParam p;
  p.name = name;
  p.type = ParamType::String;
  p.description = description;
  p.required = required;
  p.enumValues = std::move(allowed);
  tool().params.push_back(std::move(p));
  return *this;
}

ToolBuilder& ToolBuilder::mutating(bool value) {
  tool().mutating = value;
  return *this;
}

ToolBuilder& ToolBuilder::allowWrite(bool value) {
  tool().writeAllowed = value;
  return *this;
}

ToolBuilder& ToolBuilder::onCall(ToolHandler handler) {
  tool().handler = std::move(handler);
  return *this;
}

// ---------------- Registry ----------------

ToolBuilder ToolRegistry::addTool(const std::string& name, const std::string& description) {
  Tool t;
  t.name = name;
  t.description = description;
  tools_.push_back(std::move(t));
  return ToolBuilder(this, tools_.size() - 1);
}

const Tool* ToolRegistry::find(const std::string& name) const {
  for (const auto& t : tools_) {
    if (t.name == name) return &t;
  }
  return nullptr;
}

namespace {
bool typeMatches(ParamType type, JsonVariantConst v) {
  switch (type) {
    case ParamType::String: return v.is<const char*>();
    case ParamType::Integer: return v.is<long>();
    case ParamType::Number: return v.is<float>();
    case ParamType::Boolean: return v.is<bool>();
    case ParamType::Object: return v.is<JsonObjectConst>();
    case ParamType::Array: return v.is<JsonArrayConst>();
  }
  return false;
}
}  // namespace

Status ToolRegistry::validate(const Tool& tool, const std::string& argsJson) const {
  JsonDocument doc;
  if (!argsJson.empty()) {
    if (deserializeJson(doc, argsJson)) return Status::fail(Error::SchemaValidationFailed);
  }
  for (const auto& p : tool.params) {
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

Result<ToolResult> ToolRegistry::dispatch(const std::string& name,
                                          const std::string& argsJson) const {
  const Tool* t = find(name);
  if (t == nullptr) return Result<ToolResult>::fail(Error::NotFound);
  if (!t->hasHandler()) return Result<ToolResult>::fail(Error::InvalidState);

  if (!validate(*t, argsJson).isOk()) {
    return Result<ToolResult>::ok(
        ToolResult::error("invalid arguments for tool '" + name + "'"));
  }
  ToolCallArgs args(argsJson);
  return Result<ToolResult>::ok(t->handler(args));
}

}  // namespace edge
