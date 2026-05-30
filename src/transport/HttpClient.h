// EdgeLLM — blocking HTTP/1.1 client over an abstract IConnection.
// Arduino-independent: it drives any IConnection (a fake in tests, a TLS
// `Client` adapter on-device) and parses the response, handling both
// Content-Length and chunked transfer encoding with a bounded body size.
//
// Streaming responses (SSE) are layered on top of the same machinery in a later
// phase via SseParser; Phase 1 provides the blocking request/response path used
// by health checks and non-streaming API calls.
#ifndef EDGELLM_TRANSPORT_HTTPCLIENT_H
#define EDGELLM_TRANSPORT_HTTPCLIENT_H

#include <cstdint>
#include <functional>

#include "../core/Logger.h"
#include "HttpTypes.h"
#include "IConnection.h"

namespace edge {

class HttpClient {
 public:
  // Monotonic millisecond clock. On-device this is wired to millis(); in tests a
  // fake clock is injected to exercise timeouts deterministically. When null,
  // timeouts are disabled (the loop relies on the connection making progress).
  using ClockFn = uint32_t (*)();

  // Streaming body sink. Receives decoded body bytes as they arrive (chunked
  // transfer-encoding is already undone). Return false to abort the stream
  // early (e.g. the caller saw a completion sentinel). Total decoded bytes are
  // still bounded by maxResponseBody.
  using BodyChunkFn = std::function<bool(const char* data, size_t len)>;

  // The client borrows (does not own) the connection and optional logger.
  explicit HttpClient(IConnection& conn, Logger* logger = nullptr);

  void setClock(ClockFn clock) { clock_ = clock; }
  void setTimeout(uint32_t ms) { timeoutMs_ = ms; }
  void setMaxResponseBody(uint32_t bytes) { maxResponseBody_ = bytes; }
  void setReceiveBufferSize(uint16_t bytes) { recvBufferSize_ = bytes < 64 ? 64 : bytes; }

  // Performs the request and fills `out`. Connects first if the connection is
  // not already open. Returns Ok on a complete HTTP exchange regardless of the
  // HTTP status code (inspect out.status); returns a transport/parse error
  // otherwise.
  Status send(const HttpRequest& req, HttpResponse& out);

  // Like send(), but delivers the body incrementally to `onChunk` instead of
  // buffering it. `outHeaders` is populated with status/reason/headers (body
  // stays empty). Used for SSE/streaming LLM responses to keep RAM bounded.
  Status sendStream(const HttpRequest& req, HttpResponse& outHeaders, const BodyChunkFn& onChunk);

 private:
  Status writeAll(const std::string& data, uint32_t startMs);
  Status readHeaders(std::string& leftoverBody, HttpResponse& out, uint32_t startMs);
  Status readBody(const std::string& initial, HttpResponse& out, uint32_t startMs);
  bool timedOut(uint32_t startMs) const;
  uint32_t now() const { return clock_ ? clock_() : 0; }

  IConnection& conn_;
  Logger* logger_;
  ClockFn clock_ = nullptr;
  uint32_t timeoutMs_ = 15000;
  uint32_t maxResponseBody_ = 16384;
  uint16_t recvBufferSize_ = 512;
};

}  // namespace edge

#endif  // EDGELLM_TRANSPORT_HTTPCLIENT_H
