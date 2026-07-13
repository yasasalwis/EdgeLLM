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

#include <cstdint>

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
#include "UsageMeter.h"

namespace edge {

// Controls the on-device agent loop (tool calling).
struct AgentOptions {
  // Hard cap on model<->tool round-trips, so a misbehaving model can't loop
  // forever on a microcontroller.
  uint8_t maxIterations = 6;
};

// Retry policy for transient failures: transport errors (connect/DNS/TLS/
// timeout/dropped socket) and HTTP 429/500/502/503/504. Auth errors, 400s and
// schema failures are never retried here (schema failures have their own
// retry). Waits double per attempt: initialBackoffMs, 2x, 4x ... capped at
// maxBackoffMs.
struct RetryPolicy {
  uint8_t maxRetries = 2;           // extra attempts after the first; 0 disables
  uint32_t initialBackoffMs = 500;  // first wait before retrying
  uint32_t maxBackoffMs = 8000;     // ceiling for backoff and Retry-After waits
  bool respectRetryAfter = true;    // honor a numeric Retry-After response header
  bool jitter = true;               // randomize each wait within [half, full]
};

// Lightweight observability counters, updated by every call. Read with
// LLMClient::metrics(); expose them over MCP or print them periodically to
// watch a deployed device's health. Latency fields need a clock.
struct ClientMetrics {
  uint32_t requests = 0;         // HTTP requests attempted (incl. retries)
  uint32_t retries = 0;          // transient-failure retries performed
  uint32_t transportErrors = 0;  // requests that ended in a transport error
  uint32_t httpErrors = 0;       // final responses with a non-2xx status
  uint32_t schemaRetries = 0;    // model outputs rejected by validation, then retried
  uint32_t inputTokens = 0;      // provider-reported usage, accumulated
  uint32_t outputTokens = 0;
  uint32_t lastLatencyMs = 0;   // round-trip of the most recent request
  uint32_t totalLatencyMs = 0;  // summed round-trips
};

class LLMClient {
 public:
  LLMClient(Provider& provider, IConnection& conn, Logger* logger = nullptr);

  // Millisecond sleep used between retry attempts. On Arduino pass
  // edge::edgeArduinoDelay (yields to the scheduler). When unset, the client
  // busy-waits on the clock if one is set, or retries immediately otherwise.
  using DelayFn = void (*)(uint32_t ms);

  // Transport tuning (forwarded to HttpClient per request).
  void setClock(HttpClient::ClockFn clock) { clock_ = clock; }
  void setTimeout(uint32_t ms) { timeoutMs_ = ms; }
  void setMaxResponseBody(uint32_t bytes) { maxResponseBody_ = bytes; }
  void setDelayFn(DelayFn fn) { delayFn_ = fn; }

  // Retry tuning; mutate in place, e.g. client.retryPolicy().maxRetries = 0;
  RetryPolicy& retryPolicy() { return retryPolicy_; }
  const RetryPolicy& retryPolicy() const { return retryPolicy_; }

  // Optional spend guard (borrowed, not owned). When a cap is exhausted every
  // call fails fast with Error::BudgetExceeded until the meter is reset.
  void setUsageMeter(UsageMeter* meter) { meter_ = meter; }

  // Observability counters (requests, retries, errors, tokens, latency).
  const ClientMetrics& metrics() const { return metrics_; }
  void resetMetrics() { metrics_ = ClientMetrics(); }

  // Extra attempts if the model returns JSON that fails schema validation.
  void setStructuredRetries(uint8_t retries) { structuredRetries_ = retries; }

  // Connection reuse. Within one generate()/run() call the TLS connection is
  // always kept alive across the requests it makes (agent-loop rounds,
  // validation retries) — that alone saves one full TCP+TLS handshake per
  // round. By default the connection is closed when the call returns so no
  // socket/RAM is held while idle. Persistent mode keeps it open between calls
  // too: faster for frequent calls, at the cost of an idle socket the server
  // may close (which is handled transparently by a reconnect).
  void setPersistentConnection(bool persistent) { persistentConn_ = persistent; }

  // Default options applied to every call; mutate to set model/system/etc.
  ChatOptions& options() { return options_; }
  const ChatOptions& options() const { return options_; }

  // --- Structured generation (the only response mode) ---
  // The model returns a JSON object validated against `schema`.
  Result<StructuredResult> generate(const ResponseSchema& schema, const std::string& system,
                                    const std::string& user);
  // Vision: same, with a base64-encoded image (e.g. an ESP32-CAM JPEG frame)
  // attached to the user message. `imageMime` e.g. "image/jpeg". See Message
  // for the memory guidance on image sizes.
  Result<StructuredResult> generate(const ResponseSchema& schema, const std::string& system,
                                    const std::string& user, const std::string& imageBase64,
                                    const std::string& imageMime);
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
  Result<StructuredResult> runImpl(const ResponseSchema& schema, const MessageList& messages,
                                   const ToolRegistry& tools);
  // Ends one public generate()/run() cycle: closes the connection unless the
  // caller opted into a persistent one.
  void finishRequestCycle();
  // http.send() plus the RetryPolicy: transient transport errors and retryable
  // HTTP statuses are re-attempted with backoff. Returns Ok whenever a final
  // HTTP response is available in `resp` (its status may still be an error the
  // caller maps), or the last transport error once retries are exhausted.
  Status sendWithRetry(HttpClient& http, const HttpRequest& req, HttpResponse& resp);
  void wait(uint32_t ms);
  uint32_t backoffFor(uint8_t attempt, const HttpResponse& resp, bool haveResponse);
  void configure(HttpClient& http) const;
  static Error mapStatus(int httpStatus);

  Provider& provider_;
  IConnection& conn_;
  Logger* logger_;
  ChatOptions options_;
  AgentOptions agentOptions_;
  HttpClient::ClockFn clock_ = nullptr;
  DelayFn delayFn_ = nullptr;
  RetryPolicy retryPolicy_;
  UsageMeter* meter_ = nullptr;
  ClientMetrics metrics_;
  uint32_t timeoutMs_ = 20000;
  uint32_t maxResponseBody_ = 32768;
  uint32_t jitterState_ = 0x6d2b79f5u;  // cheap LCG state for backoff jitter
  uint8_t structuredRetries_ = 1;
  bool persistentConn_ = false;
};

}  // namespace edge

#endif  // EDGELLM_LLM_LLMCLIENT_H
