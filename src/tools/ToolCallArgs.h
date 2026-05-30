// EdgeLLM — typed accessor over a tool call's JSON arguments.
// Arduino-independent (uses ArduinoJson). Handlers receive one of these and pull
// out typed values without touching ArduinoJson directly.
#ifndef EDGELLM_TOOLS_TOOLCALLARGS_H
#define EDGELLM_TOOLS_TOOLCALLARGS_H

#include <ArduinoJson.h>

#include <string>

namespace edge {

class ToolCallArgs {
 public:
  // Parses `json` (expected to be a JSON object). If parsing fails or it is not
  // an object, ok() returns false and all getters return their defaults.
  explicit ToolCallArgs(const std::string& json);

  bool ok() const { return ok_; }
  bool has(const char* key) const { return ok_ && !doc_[key].isNull(); }

  std::string getString(const char* key, const std::string& def = "") const;
  long getInt(const char* key, long def = 0) const;
  double getNumber(const char* key, double def = 0.0) const;
  bool getBool(const char* key, bool def = false) const;

  // Read-only view of the underlying object for advanced handlers.
  JsonObjectConst raw() const { return doc_.as<JsonObjectConst>(); }

 private:
  JsonDocument doc_;
  bool ok_ = false;
};

}  // namespace edge

#endif  // EDGELLM_TOOLS_TOOLCALLARGS_H
