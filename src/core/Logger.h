// EdgeLLM — structured, redacting logger.
// Arduino-independent core. Output is delegated to an ILogSink so the same
// logger works on-device (Serial sink) and in native tests (capture sink).
//
// Security: registered secrets (API keys, bearer tokens, WiFi passwords) are
// scrubbed from every message before it reaches a sink. This is defense in
// depth — callers should still avoid logging secrets, but if one slips through
// the logger masks it.
#ifndef EDGELLM_CORE_LOGGER_H
#define EDGELLM_CORE_LOGGER_H

#include <string>
#include <vector>

namespace edge {

enum class LogLevel : uint8_t {
  Debug = 0,
  Info = 1,
  Warn = 2,
  Error = 3,
  None = 4,  // disables all output
};

const char* logLevelName(LogLevel level);

// Sink interface — implement to route log lines somewhere (Serial, a file, a
// network collector, a test buffer). `line` is already level-prefixed and
// redacted.
class ILogSink {
 public:
  virtual ~ILogSink() = default;
  virtual void write(LogLevel level, const std::string& line) = 0;
};

class Logger {
 public:
  Logger() = default;

  // Lines below `level` are dropped before any formatting work is done.
  void setLevel(LogLevel level) { level_ = level; }
  LogLevel level() const { return level_; }

  // The logger does not own the sink. Pass nullptr to disable output entirely.
  void setSink(ILogSink* sink) { sink_ = sink; }

  // Register a secret value to be scrubbed from all future log lines. Empty and
  // very short (<4 char) values are ignored to avoid masking common substrings.
  void registerSecret(const std::string& secret);
  void clearSecrets() { secrets_.clear(); }

  void log(LogLevel level, const std::string& message);
  void debug(const std::string& message) { log(LogLevel::Debug, message); }
  void info(const std::string& message) { log(LogLevel::Info, message); }
  void warn(const std::string& message) { log(LogLevel::Warn, message); }
  void error(const std::string& message) { log(LogLevel::Error, message); }

  // Replaces every registered secret occurrence in `text` with a fixed mask.
  // Exposed for callers that want to sanitize a value before using it elsewhere.
  std::string redact(const std::string& text) const;

  // Static helper: mask all but the last `keep` characters of a single token,
  // e.g. maskToken("sk-ant-api03-abcd1234") -> "***1234". Never reveals length.
  static std::string maskToken(const std::string& token, size_t keep = 4);

 private:
  LogLevel level_ = LogLevel::Info;
  ILogSink* sink_ = nullptr;
  std::vector<std::string> secrets_;
};

}  // namespace edge

#endif  // EDGELLM_CORE_LOGGER_H
