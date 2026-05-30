#include "Logger.h"

namespace edge {

namespace {
constexpr char kMask[] = "***";
constexpr size_t kMinSecretLen = 4;
}  // namespace

const char* logLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::None: return "NONE";
  }
  return "?";
}

void Logger::registerSecret(const std::string& secret) {
  if (secret.size() < kMinSecretLen) return;
  for (const auto& existing : secrets_) {
    if (existing == secret) return;  // de-dupe
  }
  secrets_.push_back(secret);
}

std::string Logger::redact(const std::string& text) const {
  if (secrets_.empty()) return text;
  std::string out = text;
  for (const auto& secret : secrets_) {
    size_t pos = 0;
    while ((pos = out.find(secret, pos)) != std::string::npos) {
      out.replace(pos, secret.size(), kMask);
      pos += sizeof(kMask) - 1;
    }
  }
  return out;
}

std::string Logger::maskToken(const std::string& token, size_t keep) {
  if (token.empty()) return std::string(kMask);
  if (token.size() <= keep) return std::string(kMask);
  return std::string(kMask) + token.substr(token.size() - keep);
}

void Logger::log(LogLevel level, const std::string& message) {
  if (level < level_ || level_ == LogLevel::None) return;
  if (sink_ == nullptr) return;
  std::string line = "[";
  line += logLevelName(level);
  line += "] ";
  line += redact(message);
  sink_->write(level, line);
}

}  // namespace edge
