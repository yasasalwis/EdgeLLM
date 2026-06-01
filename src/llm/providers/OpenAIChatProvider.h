// EdgeLLM — OpenAI Chat Completions wire format.
// Arduino-independent (uses ArduinoJson). Implements the /v1/chat/completions
// request/response/stream shape used by OpenAI and a large ecosystem of
// compatible servers (Groq, OpenRouter, LM Studio, llama.cpp, vLLM, ...). The
// endpoint and auth header are configurable, which is exactly the "generic
// OpenAI-compatible adapter" from discovery. OpenAIProvider is a thin preset.
#ifndef EDGELLM_LLM_OPENAICHATPROVIDER_H
#define EDGELLM_LLM_OPENAICHATPROVIDER_H

#include "../Provider.h"

namespace edge {

class OpenAIChatProvider : public Provider {
 public:
  struct Endpoint {
    std::string host = "api.openai.com";
    uint16_t port = 443;
    bool secure = true;
    std::string path = "/v1/chat/completions";
    std::string authHeader = "Authorization";  // header name carrying the key
    std::string authPrefix = "Bearer ";        // value prefix before the key
  };

  OpenAIChatProvider(std::string apiKey, std::string defaultModel, Endpoint endpoint,
                     const char* providerName = "openai-compatible");

  const char* name() const override { return name_; }
  bool secure() const override { return endpoint_.secure; }

  Status buildStructuredRequest(const MessageList& messages, const ChatOptions& options,
                                const ResponseSchema& schema, HttpRequest& out) override;
  Status parseStructuredResponse(const HttpResponse& response, std::string& jsonOut,
                                 ChatResult& meta) override;

  bool supportsTools() const override { return true; }
  Status buildToolRequest(const MessageList& messages, const ChatOptions& options,
                          const ToolRegistry& tools, HttpRequest& out) override;
  Status parseToolResponse(const HttpResponse& response, AgentTurn& out) override;

 protected:
  std::string apiKey_;
  std::string defaultModel_;
  Endpoint endpoint_;
  const char* name_;
};

// Preset for OpenAI itself.
class OpenAIProvider : public OpenAIChatProvider {
 public:
  explicit OpenAIProvider(std::string apiKey, std::string model = "gpt-4o-mini")
      : OpenAIChatProvider(std::move(apiKey), std::move(model), Endpoint{}, "openai") {}
};

}  // namespace edge

#endif  // EDGELLM_LLM_OPENAICHATPROVIDER_H
