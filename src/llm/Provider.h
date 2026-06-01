// EdgeLLM — LLM provider abstraction.
// Arduino-independent. EdgeLLM's LLM client produces STRUCTURED OUTPUT only: the
// caller supplies system/user messages plus a ResponseSchema, and the model
// returns a JSON object validated against it. A Provider knows how to build the
// schema-constrained request for its API and extract the JSON the model
// produced; the client validates it.
//
// Adding a new backend = implementing this interface (the "pluggable" provider
// support promised in discovery).
#ifndef EDGELLM_LLM_PROVIDER_H
#define EDGELLM_LLM_PROVIDER_H

#include "../core/Result.h"
#include "../tools/ToolRegistry.h"
#include "../transport/HttpTypes.h"
#include "ChatTypes.h"
#include "Message.h"
#include "ResponseSchema.h"

namespace edge {

class Provider {
 public:
  virtual ~Provider() = default;

  // Short identifier, e.g. "anthropic", "openai", "gemini", "ollama".
  virtual const char* name() const = 0;

  // True if the endpoint uses TLS (cloud APIs). Local Ollama may be false.
  virtual bool secure() const = 0;

  // Builds a request that constrains the model to return JSON matching `schema`,
  // using the provider's native structured-output mechanism. Serializes the
  // system/user/assistant message history into `out`.
  virtual Status buildStructuredRequest(const MessageList& messages, const ChatOptions& options,
                                        const ResponseSchema& schema, HttpRequest& out) = 0;

  // Extracts the model's JSON output from a successful (2xx) response into
  // `jsonOut`, and any usage/finish metadata into `meta`. The client validates
  // `jsonOut` against the schema afterwards.
  virtual Status parseStructuredResponse(const HttpResponse& response, std::string& jsonOut,
                                         ChatResult& meta) = 0;

  // --- Tool/function calling (agent loop) ---
  // Providers that support tools override these. The agent loop only runs when
  // supportsTools() is true; otherwise it returns Error::NotImplemented.

  virtual bool supportsTools() const { return false; }

  // Builds a non-streaming chat request that advertises `tools` and serializes a
  // message history which may include assistant tool-call turns and Role::Tool
  // results. Default: not implemented.
  virtual Status buildToolRequest(const MessageList& messages, const ChatOptions& options,
                                  const ToolRegistry& tools, HttpRequest& out) {
    (void)messages;
    (void)options;
    (void)tools;
    (void)out;
    return Status::fail(Error::NotImplemented);
  }

  // Parses a tool-enabled response into an AgentTurn (text + requested calls).
  virtual Status parseToolResponse(const HttpResponse& response, AgentTurn& out) {
    (void)response;
    (void)out;
    return Status::fail(Error::NotImplemented);
  }
};

}  // namespace edge

#endif  // EDGELLM_LLM_PROVIDER_H
