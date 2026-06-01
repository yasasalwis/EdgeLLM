// EdgeLLM — MCP prompt registry.
// Arduino-independent. A prompt is a named, parameterized message template the
// host can fetch (prompts/get) and present to its model. Handlers receive the
// supplied arguments and return the prompt text.
#ifndef EDGELLM_MCP_PROMPTREGISTRY_H
#define EDGELLM_MCP_PROMPTREGISTRY_H

#include <functional>
#include <string>
#include <vector>

#include "../core/Result.h"
#include "../tools/ToolCallArgs.h"

namespace edge {

struct PromptArg {
  std::string name;
  std::string description;
  bool required = false;
};

struct Prompt {
  std::string name;
  std::string description;
  std::vector<PromptArg> args;
  // Returns the rendered prompt text given the supplied arguments.
  std::function<Result<std::string>(ToolCallArgs&)> onGet;
};

class PromptRegistry;

class PromptBuilder {
 public:
  PromptBuilder& arg(const std::string& name, const std::string& description,
                     bool required = false);
  PromptBuilder& onGet(std::function<Result<std::string>(ToolCallArgs&)> handler);

 private:
  friend class PromptRegistry;
  PromptBuilder(PromptRegistry* reg, size_t index) : registry_(reg), index_(index) {}
  Prompt& prompt();
  PromptRegistry* registry_;
  size_t index_;
};

class PromptRegistry {
 public:
  PromptBuilder addPrompt(const std::string& name, const std::string& description);

  const std::vector<Prompt>& prompts() const { return prompts_; }
  const Prompt* find(const std::string& name) const;
  size_t size() const { return prompts_.size(); }
  bool empty() const { return prompts_.empty(); }

  // Renders a prompt by name with the given JSON arguments.
  Result<std::string> get(const std::string& name, const std::string& argsJson) const;

 private:
  friend class PromptBuilder;
  std::vector<Prompt> prompts_;
};

}  // namespace edge

#endif  // EDGELLM_MCP_PROMPTREGISTRY_H
