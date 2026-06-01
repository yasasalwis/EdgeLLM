// EdgeLLM — high-level LLM client (Feature A), structured output only.
// Arduino-independent: orchestrates a Provider over an injected IConnection via
// HttpClient. The model is always constrained to return a JSON object matching a
// caller-supplied ResponseSchema, which the client validates (with a retry).
// There is no free-text chat or text streaming — structured output is the only
// response mode. This feature is fully usable WITHOUT the MCP server.
//
// The caller supplies the connection (a secure or plain one matching the
// provider) so the client stays testable with a fake socket and so TLS trust is
// configured where the sketch can see it.
#ifndef EDGELLM_LLM_LLMCLIENT_H
#define EDGELLM_LLM_LLMCLIENT_H

#include "../core/Logger.h"
#include "../tools/ToolRegistry.h"
#include "../transport/HttpClient.h"
#include "../transport/IConnection.h"
#include "ChatTypes.h"
#include "Conversation.h"
#include "Message.h"
#include "Provider.h"
#include "ResponseSchema.h"
#include "StructuredResult.h"

namespace edge {

// Controls the on-device agent loop (tool calling).
struct AgentOptions {
  // Hard cap on model<->tool round-trips, so a misbehaving model can't loop
  // forever on a microcontroller.
  uint8_t maxIterations = 6;
};

class LLMClient {
 public:
  LLMClient(Provider& provider, IConnection& conn, Logger* logger = nullptr);

  // Transport tuning (forwarded to HttpClient per request).
  void setClock(HttpClient::ClockFn clock) { clock_ = clock; }
  void setTimeout(uint32_t ms) { timeoutMs_ = ms; }
  void setMaxResponseBody(uint32_t bytes) { maxResponseBody_ = bytes; }

  // Extra attempts if the model returns JSON that fails schema validation.
  void setStructuredRetries(uint8_t retries) { structuredRetries_ = retries; }

  // Default options applied to every call; mutate to set model/system/etc.
  ChatOptions& options() { return options_; }
  const ChatOptions& options() const { return options_; }

  // --- Structured generation (the only response mode) ---
  // The model returns a JSON object validated against `schema`.
  Result<StructuredResult> generate(const ResponseSchema& schema, const std::string& system,
                                    const std::string& user);
  Result<StructuredResult> generate(const ResponseSchema& schema, const MessageList& messages);
  // Appends the assistant's JSON output to `convo` on success.
  Result<StructuredResult> generate(const ResponseSchema& schema, Conversation& convo);

  // --- Agent loop (tool calling, structured final answer) ---
  // Runs the model with `tools` available, executing any tools it requests, then
  // produces a final answer validated against `schema`. Requires
  // provider.supportsTools(); otherwise Error::NotImplemented.
  AgentOptions& agentOptions() { return agentOptions_; }
  Result<StructuredResult> run(const ResponseSchema& schema, const std::string& prompt,
                               const ToolRegistry& tools);
  Result<StructuredResult> run(const ResponseSchema& schema, const MessageList& messages,
                               const ToolRegistry& tools);

 private:
  Result<StructuredResult> doGenerate(const ResponseSchema& schema, const MessageList& messages,
                                      const ChatOptions& opts);
  void configure(HttpClient& http) const;
  static Error mapStatus(int httpStatus);

  Provider& provider_;
  IConnection& conn_;
  Logger* logger_;
  ChatOptions options_;
  AgentOptions agentOptions_;
  HttpClient::ClockFn clock_ = nullptr;
  uint32_t timeoutMs_ = 20000;
  uint32_t maxResponseBody_ = 32768;
  uint8_t structuredRetries_ = 1;
};

}  // namespace edge

#endif  // EDGELLM_LLM_LLMCLIENT_H
