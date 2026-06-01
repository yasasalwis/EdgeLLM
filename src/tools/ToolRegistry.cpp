#include "ToolRegistry.h"

#include <ArduinoJson.h>

#include "SchemaUtil.h"

namespace edge {

// ---------------- Schema writer ----------------

void writeToolSchema(const Tool& tool, JsonObject schemaOut) {
  writeObjectSchema(tool.params, schemaOut);
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
  // Re-registering an existing name redefines that tool in place rather than
  // creating a duplicate (a duplicate would confuse the model and break MCP
  // tools/list, which requires unique names).
  for (size_t i = 0; i < tools_.size(); ++i) {
    if (tools_[i].name == name) {
      tools_[i] = Tool{};
      tools_[i].name = name;
      tools_[i].description = description;
      return ToolBuilder(this, i);
    }
  }
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

Status ToolRegistry::validate(const Tool& tool, const std::string& argsJson) const {
  return validateObject(tool.params, argsJson);
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
