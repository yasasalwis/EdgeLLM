// EdgeLLM — a registered tool/function.
// Arduino-independent (uses ArduinoJson for schema). A Tool pairs a JSON-Schema
// description (so a model knows how to call it) with a handler that runs on the
// device. The read-only vs mutating annotation and the writeAllowed gate are
// carried here for the MCP server (Phase 4) — the agent loop runs any registered
// tool, since registering it is the sketch author's explicit opt-in.
#ifndef EDGELLM_TOOLS_TOOL_H
#define EDGELLM_TOOLS_TOOL_H

#include <functional>

#include "ToolCallArgs.h"
#include "ToolTypes.h"

namespace edge {

using ToolHandler = std::function<ToolResult(ToolCallArgs&)>;

struct Tool {
  std::string name;
  std::string description;
  std::vector<ToolParam> params;

  // Annotations surfaced to MCP hosts (readOnly/destructive hints) and used by
  // the MCP server's deny-by-default write gate. Not enforced by the agent loop.
  bool mutating = false;      // true if the tool changes state (a "write")
  bool writeAllowed = false;  // MCP: a mutating tool stays disabled until this is set

  ToolHandler handler;

  bool hasHandler() const { return static_cast<bool>(handler); }
};

// Writes the tool's input schema as a JSON Schema object into `schemaOut`:
// {"type":"object","properties":{...},"required":[...]}. Shared by every
// provider's request builder and by the MCP server.
void writeToolSchema(const Tool& tool, JsonObject schemaOut);

}  // namespace edge

#endif  // EDGELLM_TOOLS_TOOL_H
