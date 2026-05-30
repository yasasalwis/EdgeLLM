// EdgeLLM — chat message types.
// Arduino-independent. A conversation is an ordered list of Messages. The
// system prompt is carried separately in ChatOptions (most providers model it
// as a top-level field, not an in-list message), but a System role is provided
// for callers/providers that prefer it inline.
#ifndef EDGELLM_LLM_MESSAGE_H
#define EDGELLM_LLM_MESSAGE_H

#include <string>
#include <vector>

#include "../tools/ToolTypes.h"

namespace edge {

enum class Role : uint8_t {
  System = 0,
  User = 1,
  Assistant = 2,
  Tool = 3,  // a tool/function result (fed back to the model in the agent loop)
};

struct Message {
  Role role = Role::User;
  std::string content;

  // For an Assistant turn that requested tools: the calls it made (Phase 3).
  std::vector<ToolCall> toolCalls;

  // For a Role::Tool message: which call this answers, and the tool's name.
  std::string toolCallId;
  std::string toolName;
  bool isToolError = false;

  Message() = default;
  Message(Role r, std::string c) : role(r), content(std::move(c)) {}

  static Message user(std::string c) { return Message(Role::User, std::move(c)); }
  static Message assistant(std::string c) { return Message(Role::Assistant, std::move(c)); }
  static Message system(std::string c) { return Message(Role::System, std::move(c)); }

  // Builds a tool-result message answering `call`.
  static Message toolResult(const ToolCall& call, const ToolResult& result) {
    Message m(Role::Tool, result.content);
    m.toolCallId = call.id;
    m.toolName = call.name;
    m.isToolError = result.isError;
    return m;
  }
};

using MessageList = std::vector<Message>;

}  // namespace edge

#endif  // EDGELLM_LLM_MESSAGE_H
