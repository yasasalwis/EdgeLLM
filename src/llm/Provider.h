// EdgeLLM — LLM provider abstraction.
// Arduino-independent. A Provider knows how to (a) turn messages + options into
// an HTTP request for a specific API, (b) parse a non-streaming JSON response,
// and (c) parse one streamed payload (an SSE data field or an NDJSON line) into
// a delta. LLMClient drives the transport; providers own only the wire format.
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

namespace edge {

// How a provider frames its streaming responses.
enum class StreamFormat : uint8_t {
  SSE = 0,     // text/event-stream; one payload per SSE "data:" field
  NDJSON = 1,  // newline-delimited JSON; one payload per line (e.g. Ollama)
};

class Provider {
 public:
  virtual ~Provider() = default;

  // Short identifier, e.g. "anthropic", "openai", "gemini", "ollama".
  virtual const char* name() const = 0;

  // True if the endpoint uses TLS (cloud APIs). Local Ollama may be false.
  virtual bool secure() const = 0;

  virtual StreamFormat streamFormat() const { return StreamFormat::SSE; }

  // Fills `out` (method, host, port, path, headers, body) for a chat request.
  // When `stream` is true the request asks the API to stream.
  virtual Status buildChatRequest(const MessageList& messages, const ChatOptions& options,
                                  bool stream, HttpRequest& out) = 0;

  // Parses a successful (2xx) non-streaming response body into `out`.
  virtual Status parseChatResponse(const HttpResponse& response, ChatResult& out) = 0;

  // Parses a single streamed payload into `out`. `payload` is the raw JSON of
  // one SSE data field or one NDJSON line. Implementations set out.textDelta and
  // out.done as appropriate; non-JSON keep-alive payloads should yield an empty
  // delta and Status::ok().
  virtual Status parseStreamEvent(const std::string& payload, StreamDelta& out) = 0;

  // --- Tool/function calling (Phase 3) ---
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
