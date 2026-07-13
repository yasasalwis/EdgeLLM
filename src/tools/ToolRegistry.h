// EdgeLLM — registry of tools, with a fluent builder.
// Arduino-independent. This is the shared surface from discovery: register a
// tool once here, then expose it to the LLM agent loop and/or the MCP server.
//
// Usage:
//   registry.addTool("get_temp", "Read the room temperature")
//           .param("unit", edge::ParamType::String, "celsius or fahrenheit", false)
//           .onCall([](edge::ToolCallArgs& a) {
//             return edge::ToolResult::ok("21.5");
//           });
#ifndef EDGELLM_TOOLS_TOOLREGISTRY_H
#define EDGELLM_TOOLS_TOOLREGISTRY_H

#include "../core/Result.h"
#include "FieldSpec.h"
#include "Tool.h"

namespace edge {

class ToolRegistry;

// Fluent configurator returned by ToolRegistry::addTool. Each method returns
// *this so calls chain. The builder edits the tool in place in the registry.
class ToolBuilder {
 public:
  ToolBuilder& param(const std::string& name, ParamType type, const std::string& description,
                     bool required = true);
  ToolBuilder& paramEnum(const std::string& name, const std::string& description,
                         std::vector<std::string> allowed, bool required = true);
  // A nested object parameter whose properties are `shape`'s fields.
  ToolBuilder& paramObject(const std::string& name, const std::string& description,
                           const FieldSpec& shape, bool required = true);
  // An array parameter of primitive elements (string/number/integer/boolean).
  ToolBuilder& paramArray(const std::string& name, const std::string& description,
                          ParamType itemType, bool required = true);
  // An array parameter of objects, each matching `itemShape`.
  ToolBuilder& paramArray(const std::string& name, const std::string& description,
                          const FieldSpec& itemShape, bool required = true);
  ToolBuilder& mutating(bool value = true);    // mark as a state-changing "write"
  ToolBuilder& allowWrite(bool value = true);  // MCP gate: permit the write
  ToolBuilder& onCall(ToolHandler handler);

 private:
  friend class ToolRegistry;
  ToolBuilder(ToolRegistry* registry, size_t index) : registry_(registry), index_(index) {}
  Tool& tool();

  ToolRegistry* registry_;
  size_t index_;
};

class ToolRegistry {
 public:
  ToolBuilder addTool(const std::string& name, const std::string& description);

  bool has(const std::string& name) const { return find(name) != nullptr; }
  const Tool* find(const std::string& name) const;
  size_t size() const { return tools_.size(); }
  bool empty() const { return tools_.empty(); }
  const std::vector<Tool>& tools() const { return tools_; }

  // Validates a JSON arguments object against the tool's schema (required keys
  // present, types match, enum membership). Returns SchemaValidationFailed with
  // no side effects on mismatch.
  Status validate(const Tool& tool, const std::string& argsJson) const;

  // Looks up `name`, validates `argsJson`, and runs the handler. A validation
  // failure yields a ToolResult marked isError (so the agent loop can feed it
  // back to the model). Returns Error::NotFound if the tool doesn't exist and
  // Error::InvalidState if it has no handler.
  Result<ToolResult> dispatch(const std::string& name, const std::string& argsJson) const;

 private:
  friend class ToolBuilder;
  std::vector<Tool> tools_;
};

}  // namespace edge

#endif  // EDGELLM_TOOLS_TOOLREGISTRY_H
