// EdgeLLM — Ollama provider (local LLM runtime).
// Arduino-independent (uses ArduinoJson). Targets POST /api/chat on a local
// Ollama server. Defaults to plain HTTP on the LAN (no auth, no TLS) — set
// secure=true only if you front Ollama with TLS. Structured output uses Ollama's
// `format` parameter (a JSON schema).
#ifndef EDGELLM_LLM_OLLAMAPROVIDER_H
#define EDGELLM_LLM_OLLAMAPROVIDER_H

#include <cstdint>

#include "../Provider.h"

namespace edge {

class OllamaProvider : public Provider {
 public:
  OllamaProvider(std::string host = "127.0.0.1", uint16_t port = 11434,
                 std::string defaultModel = "llama3.2", bool secure = false)
      : host_(std::move(host)),
        port_(port),
        defaultModel_(std::move(defaultModel)),
        secure_(secure) {}

  const char* name() const override { return "ollama"; }
  bool secure() const override { return secure_; }

  Status buildStructuredRequest(const MessageList& messages, const ChatOptions& options,
                                const ResponseSchema& schema, HttpRequest& out) override;
  Status parseStructuredResponse(const HttpResponse& response, std::string& jsonOut,
                                 ChatResult& meta) override;

  bool supportsTools() const override { return true; }
  Status buildToolRequest(const MessageList& messages, const ChatOptions& options,
                          const ToolRegistry& tools, HttpRequest& out) override;
  Status parseToolResponse(const HttpResponse& response, AgentTurn& out) override;

 private:
  std::string host_;
  uint16_t port_;
  std::string defaultModel_;
  bool secure_;
};

}  // namespace edge

#endif  // EDGELLM_LLM_OLLAMAPROVIDER_H
