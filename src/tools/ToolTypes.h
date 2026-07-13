// EdgeLLM — shared tool/function-calling value types.
// Arduino-independent. These types are the common currency between the LLM
// agent loop (Feature A, Phase 3) and the MCP server (Feature B, Phase 4): a
// tool registered once can be exposed to either, per the discovery decision.
#ifndef EDGELLM_TOOLS_TOOLTYPES_H
#define EDGELLM_TOOLS_TOOLTYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace edge {

// JSON Schema primitive types we support for tool parameters.
enum class ParamType : uint8_t { String, Number, Integer, Boolean, Object, Array };

// Maps a ParamType to its JSON Schema "type" keyword.
inline const char* paramTypeName(ParamType t) {
  switch (t) {
    case ParamType::String:
      return "string";
    case ParamType::Number:
      return "number";
    case ParamType::Integer:
      return "integer";
    case ParamType::Boolean:
      return "boolean";
    case ParamType::Object:
      return "object";
    case ParamType::Array:
      return "array";
  }
  return "string";
}

struct ToolParam {
  std::string name;
  std::string description;
  ParamType type = ParamType::String;
  bool required = true;
  std::vector<std::string> enumValues;  // optional allowed string values

  // Nested shape. For type == Object, `children` are its properties (empty
  // means "any object"). For type == Array with hasItems set, `itemType` is the
  // element type and, when itemType == Object, `children` describe the element's
  // properties. hasItems == false means "any array". Depth is bounded by
  // construction: nesting only goes as deep as the caller explicitly builds.
  std::vector<ToolParam> children;
  ParamType itemType = ParamType::String;
  bool hasItems = false;
};

// A tool invocation requested by the model.
struct ToolCall {
  std::string id;             // provider call id (OpenAI tool_call_id / Anthropic tool_use id)
  std::string name;           // tool name
  std::string argumentsJson;  // raw JSON object of arguments
};

// The outcome of running a tool, fed back to the model.
struct ToolResult {
  std::string content;   // textual or JSON result
  bool isError = false;  // true if the tool failed (model is told so)

  // Explicit constructors (not aggregate init): a default member initializer
  // makes this a non-aggregate under -std=gnu++11 (used by some Arduino cores),
  // so brace-aggregate construction would not compile there.
  ToolResult() = default;
  ToolResult(std::string c, bool err) : content(std::move(c)), isError(err) {}

  static ToolResult ok(std::string text) { return ToolResult(std::move(text), false); }
  static ToolResult error(std::string text) { return ToolResult(std::move(text), true); }
};

// One assistant turn parsed from a (non-streaming) tool-enabled response.
struct AgentTurn {
  std::string text;                 // any assistant text in this turn
  std::vector<ToolCall> toolCalls;  // tools the model wants run (may be empty)
  std::string finishReason;
  uint32_t inputTokens = 0;
  uint32_t outputTokens = 0;

  bool wantsTools() const { return !toolCalls.empty(); }
};

}  // namespace edge

#endif  // EDGELLM_TOOLS_TOOLTYPES_H
