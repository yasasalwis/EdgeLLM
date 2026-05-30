#include "ToolCallArgs.h"

namespace edge {

ToolCallArgs::ToolCallArgs(const std::string& json) {
  if (json.empty()) {
    ok_ = false;
    return;
  }
  DeserializationError err = deserializeJson(doc_, json);
  ok_ = !err && doc_.is<JsonObject>();
}

std::string ToolCallArgs::getString(const char* key, const std::string& def) const {
  if (!ok_) return def;
  const char* v = doc_[key];
  return v ? std::string(v) : def;
}

long ToolCallArgs::getInt(const char* key, long def) const {
  if (!ok_ || doc_[key].isNull()) return def;
  return doc_[key].as<long>();
}

double ToolCallArgs::getNumber(const char* key, double def) const {
  if (!ok_ || doc_[key].isNull()) return def;
  return doc_[key].as<double>();
}

bool ToolCallArgs::getBool(const char* key, bool def) const {
  if (!ok_ || doc_[key].isNull()) return def;
  return doc_[key].as<bool>();
}

}  // namespace edge
