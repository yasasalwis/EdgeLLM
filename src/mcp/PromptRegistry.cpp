#include "PromptRegistry.h"

namespace edge {

Prompt& PromptBuilder::prompt() { return registry_->prompts_[index_]; }

PromptBuilder& PromptBuilder::arg(const std::string& name, const std::string& description,
                                  bool required) {
  PromptArg a;
  a.name = name;
  a.description = description;
  a.required = required;
  prompt().args.push_back(std::move(a));
  return *this;
}

PromptBuilder& PromptBuilder::onGet(std::function<Result<std::string>(ToolCallArgs&)> handler) {
  prompt().onGet = std::move(handler);
  return *this;
}

PromptBuilder PromptRegistry::addPrompt(const std::string& name, const std::string& description) {
  Prompt p;
  p.name = name;
  p.description = description;
  prompts_.push_back(std::move(p));
  return PromptBuilder(this, prompts_.size() - 1);
}

const Prompt* PromptRegistry::find(const std::string& name) const {
  for (const auto& p : prompts_) {
    if (p.name == name) return &p;
  }
  return nullptr;
}

Result<std::string> PromptRegistry::get(const std::string& name, const std::string& argsJson) const {
  const Prompt* p = find(name);
  if (p == nullptr) return Result<std::string>::fail(Error::NotFound);
  if (!p->onGet) return Result<std::string>::fail(Error::InvalidState);
  // Validate required arguments are present.
  ToolCallArgs args(argsJson);
  for (const auto& a : p->args) {
    if (a.required && !args.has(a.name.c_str())) {
      return Result<std::string>::fail(Error::SchemaValidationFailed);
    }
  }
  return p->onGet(args);
}

}  // namespace edge
