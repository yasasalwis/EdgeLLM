#include "SseParser.h"

namespace edge {

void SseParser::reset() {
  lineBuf_.clear();
  eventType_.clear();
  dataBuf_.clear();
  lastId_.clear();
  sawData_ = false;
}

void SseParser::feed(const char* data, size_t len, const Handler& onEvent) {
  for (size_t i = 0; i < len; ++i) {
    const char c = data[i];
    if (c == '\n') {
      // Strip a trailing CR so CRLF and LF line endings behave identically.
      if (!lineBuf_.empty() && lineBuf_.back() == '\r') lineBuf_.pop_back();
      processLine(lineBuf_, onEvent);
      lineBuf_.clear();
    } else {
      lineBuf_.push_back(c);
    }
  }
}

void SseParser::processLine(const std::string& line, const Handler& onEvent) {
  if (line.empty()) {
    dispatch(onEvent);
    return;
  }
  if (line[0] == ':') {
    return;  // comment line — ignored
  }

  // Split "field:value"; a line with no colon is a field with an empty value.
  std::string field;
  std::string value;
  const size_t colon = line.find(':');
  if (colon == std::string::npos) {
    field = line;
  } else {
    field = line.substr(0, colon);
    value = line.substr(colon + 1);
    if (!value.empty() && value[0] == ' ') value.erase(0, 1);  // one leading space
  }

  if (field == "event") {
    eventType_ = value;
  } else if (field == "data") {
    dataBuf_ += value;
    dataBuf_ += '\n';
    sawData_ = true;
  } else if (field == "id") {
    // A NUL in the id is invalid per spec; ignore such ids.
    if (value.find('\0') == std::string::npos) lastId_ = value;
  }
  // "retry" and unknown fields are intentionally ignored.
}

void SseParser::dispatch(const Handler& onEvent) {
  if (!sawData_) {
    // Blank line with no data: reset the event type but emit nothing.
    eventType_.clear();
    return;
  }

  SseEvent ev;
  ev.event = eventType_.empty() ? "message" : eventType_;
  ev.data = dataBuf_;
  // Remove the single trailing '\n' the accumulator always leaves behind.
  if (!ev.data.empty() && ev.data.back() == '\n') ev.data.pop_back();
  ev.id = lastId_;

  if (onEvent) onEvent(ev);

  // Reset per-event state; lastId_ persists across events per the SSE spec.
  eventType_.clear();
  dataBuf_.clear();
  sawData_ = false;
}

}  // namespace edge
