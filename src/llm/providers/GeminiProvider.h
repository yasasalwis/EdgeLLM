// EdgeLLM — Google Gemini provider (Generative Language API).
// Arduino-independent (uses ArduinoJson). Targets
// generativelanguage.googleapis.com /v1beta/models/{model}:generateContent
// (and :streamGenerateContent?alt=sse for streaming). The API key is passed as
// a query parameter, as Gemini requires.
//
// Note: because the key is in the URL, avoid logging full request paths; the
// LLMClient registers the key with the logger so any accidental leak is masked.
#ifndef EDGELLM_LLM_GEMINIPROVIDER_H
#define EDGELLM_LLM_GEMINIPROVIDER_H

#include "../Provider.h"

namespace edge {

class GeminiProvider : public Provider {
 public:
  explicit GeminiProvider(std::string apiKey, std::string defaultModel = "gemini-1.5-flash")
      : apiKey_(std::move(apiKey)), defaultModel_(std::move(defaultModel)) {}

  const char* name() const override { return "gemini"; }
  bool secure() const override { return true; }

  Status buildStructuredRequest(const MessageList& messages, const ChatOptions& options,
                                const ResponseSchema& schema, HttpRequest& out) override;
  Status parseStructuredResponse(const HttpResponse& response, std::string& jsonOut,
                                 ChatResult& meta) override;

  bool supportsTools() const override { return true; }
  Status buildToolRequest(const MessageList& messages, const ChatOptions& options,
                          const ToolRegistry& tools, HttpRequest& out) override;
  Status parseToolResponse(const HttpResponse& response, AgentTurn& out) override;

  // Exposes the API key so LLMClient can register it for log redaction.
  const std::string& apiKey() const { return apiKey_; }

 private:
  std::string apiKey_;
  std::string defaultModel_;
};

}  // namespace edge

#endif  // EDGELLM_LLM_GEMINIPROVIDER_H
