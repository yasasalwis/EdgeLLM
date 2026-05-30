// EdgeLLM — incremental Server-Sent Events parser.
// Arduino-independent and pure. Streaming LLM responses (OpenAI, Anthropic,
// Gemini's SSE mode) arrive as SSE; this parser consumes arbitrary byte chunks
// as they come off the socket and emits one event per logical SSE record,
// without ever buffering the whole response. That bounded-memory behaviour is
// essential on boards with tens of kilobytes of RAM.
//
// Implements the dispatch rules of the WHATWG SSE specification: `event:`,
// `data:` (multiple lines joined by newlines), `id:` and comment lines, with
// dispatch on a blank line and only when a data buffer is present.
#ifndef EDGELLM_TRANSPORT_SSEPARSER_H
#define EDGELLM_TRANSPORT_SSEPARSER_H

#include <cstddef>
#include <functional>
#include <string>

namespace edge {

struct SseEvent {
  std::string event;  // event type; defaults to "message" when unspecified
  std::string data;   // payload, multi-line data joined with '\n'
  std::string id;     // last seen id (persists across events per the spec)
};

class SseParser {
 public:
  using Handler = std::function<void(const SseEvent&)>;

  // Feeds a chunk of raw bytes. Invokes `onEvent` synchronously for every
  // complete event found in (or completed by) this chunk. Partial lines are
  // retained internally until the next feed.
  void feed(const char* data, size_t len, const Handler& onEvent);

  // Discards all buffered state. Call when reusing the parser for a new stream.
  void reset();

 private:
  void processLine(const std::string& line, const Handler& onEvent);
  void dispatch(const Handler& onEvent);

  std::string lineBuf_;    // bytes of the current, not-yet-terminated line
  std::string eventType_;  // "event:" accumulator for the pending event
  std::string dataBuf_;    // "data:" accumulator for the pending event
  std::string lastId_;     // most recent "id:" value
  bool sawData_ = false;   // whether the pending event has any data line
};

}  // namespace edge

#endif  // EDGELLM_TRANSPORT_SSEPARSER_H
