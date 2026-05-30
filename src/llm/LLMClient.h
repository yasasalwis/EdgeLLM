// EdgeLLM — high-level LLM client (Feature A).
// Arduino-independent: orchestrates a Provider over an injected IConnection via
// HttpClient. Supports blocking and streaming chat, stateless or with a managed
// Conversation. This feature is fully usable WITHOUT the MCP server.
//
// The caller supplies the connection (a secure or plain one matching the
// provider) so the client stays testable with a fake socket and so TLS trust is
// configured where the sketch can see it.
#ifndef EDGELLM_LLM_LLMCLIENT_H
#define EDGELLM_LLM_LLMCLIENT_H

#include <functional>

#include "../core/Logger.h"
#include "../tools/ToolRegistry.h"
#include "../transport/HttpClient.h"
#include "../transport/IConnection.h"
#include "ChatTypes.h"
#include "Conversation.h"
#include "Message.h"
#include "Provider.h"

namespace edge {

// Controls the on-device agent loop (Feature A tool calling).
struct AgentOptions {
  // Hard cap on model<->tool round-trips, so a misbehaving model can't loop
  // forever on a microcontroller.
  uint8_t maxIterations = 6;
};

class LLMClient {
 public:
  using DeltaFn = std::function<void(const std::string& delta)>;

  LLMClient(Provider& provider, IConnection& conn, Logger* logger = nullptr);

  // Transport tuning (forwarded to HttpClient per request).
  void setClock(HttpClient::ClockFn clock) { clock_ = clock; }
  void setTimeout(uint32_t ms) { timeoutMs_ = ms; }
  void setMaxResponseBody(uint32_t bytes) { maxResponseBody_ = bytes; }

  // Default options applied to every call; mutate to set model/system/etc.
  ChatOptions& options() { return options_; }
  const ChatOptions& options() const { return options_; }

  // --- Blocking chat ---
  Result<ChatResult> chat(const std::string& userText);
  Result<ChatResult> chat(const MessageList& messages);
  // Appends the assistant reply to `convo` on success.
  Result<ChatResult> chat(Conversation& convo);

  // --- Streaming chat (onDelta is called for each text fragment) ---
  Status chatStream(const std::string& userText, const DeltaFn& onDelta,
                    ChatResult* finalOut = nullptr);
  Status chatStream(const MessageList& messages, const DeltaFn& onDelta,
                    ChatResult* finalOut = nullptr);
  // Appends the full assistant reply to `convo` on success.
  Status chatStream(Conversation& convo, const DeltaFn& onDelta, ChatResult* finalOut = nullptr);

  // --- Agent loop (tool calling) ---
  // Runs the model with `tools` available, executing any tools it requests and
  // feeding results back until it produces a final answer (or maxIterations is
  // reached). Requires provider.supportsTools(); otherwise Error::NotImplemented.
  AgentOptions& agentOptions() { return agentOptions_; }
  Result<ChatResult> run(const std::string& prompt, const ToolRegistry& tools);
  Result<ChatResult> run(const MessageList& messages, const ToolRegistry& tools);
  // Appends the final assistant answer to `convo` on success.
  Result<ChatResult> run(Conversation& convo, const ToolRegistry& tools);

 private:
  Result<ChatResult> doChat(const MessageList& messages, const ChatOptions& opts);
  Status doChatStream(const MessageList& messages, const ChatOptions& opts, const DeltaFn& onDelta,
                      ChatResult& out);
  Result<ChatResult> doRun(MessageList messages, const ChatOptions& opts, const ToolRegistry& tools);
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
};

}  // namespace edge

#endif  // EDGELLM_LLM_LLMCLIENT_H
