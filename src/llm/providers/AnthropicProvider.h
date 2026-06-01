// EdgeLLM — Anthropic Messages API provider (Claude).
// Arduino-independent (uses ArduinoJson). Targets POST /v1/messages on
// api.anthropic.com with the x-api-key + anthropic-version headers. The system
// prompt is a top-level field (not a message), and responses are content-block
// arrays — both handled here.
#ifndef EDGELLM_LLM_ANTHROPICPROVIDER_H
#define EDGELLM_LLM_ANTHROPICPROVIDER_H

#include "../Provider.h"

namespace edge {

class AnthropicProvider : public Provider {
 public:
  // Default model is a current Claude id; override per call via ChatOptions.model
  // or here. Update as Anthropic releases new models.
  explicit AnthropicProvider(std::string apiKey,
                             std::string defaultModel = "claude-haiku-4-5-20251001",
                             std::string apiVersion = "2023-06-01")
      : apiKey_(std::move(apiKey)),
        defaultModel_(std::move(defaultModel)),
        apiVersion_(std::move(apiVersion)) {}

  const char* name() const override { return "anthropic"; }
  bool secure() const override { return true; }
  Status buildStructuredRequest(const MessageList& messages, const ChatOptions& options,
                                const ResponseSchema& schema, HttpRequest& out) override;
  Status parseStructuredResponse(const HttpResponse& response, std::string& jsonOut,
                                 ChatResult& meta) override;

  bool supportsTools() const override { return true; }
  Status buildToolRequest(const MessageList& messages, const ChatOptions& options,
                          const ToolRegistry& tools, HttpRequest& out) override;
  Status parseToolResponse(const HttpResponse& response, AgentTurn& out) override;

 private:
  std::string apiKey_;
  std::string defaultModel_;
  std::string apiVersion_;
};

}  // namespace edge

#endif  // EDGELLM_LLM_ANTHROPICPROVIDER_H
