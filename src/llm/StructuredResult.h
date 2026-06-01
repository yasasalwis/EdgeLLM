// EdgeLLM — a validated structured-output result.
// Arduino-independent. Wraps the model's schema-validated JSON object and offers
// typed accessors (delegating to ToolCallArgs) plus the raw JSON string.
#ifndef EDGELLM_LLM_STRUCTUREDRESULT_H
#define EDGELLM_LLM_STRUCTUREDRESULT_H

#include <string>

#include "../tools/ToolCallArgs.h"

namespace edge {

class StructuredResult {
 public:
  // Default-constructs an empty result (needed so Result<StructuredResult> can
  // represent a failure). Accessors return defaults until a value is assigned.
  StructuredResult() : json_(), args_(json_) {}
  explicit StructuredResult(std::string json) : json_(std::move(json)), args_(json_) {}

  // The raw, validated JSON object string.
  const std::string& json() const { return json_; }

  bool has(const char* key) const { return args_.has(key); }
  std::string getString(const char* key, const std::string& def = "") const {
    return args_.getString(key, def);
  }
  long getInt(const char* key, long def = 0) const { return args_.getInt(key, def); }
  double getNumber(const char* key, double def = 0.0) const { return args_.getNumber(key, def); }
  bool getBool(const char* key, bool def = false) const { return args_.getBool(key, def); }

  // Read-only view for advanced access (nested objects/arrays).
  JsonObjectConst raw() const { return args_.raw(); }

 private:
  std::string json_;
  ToolCallArgs args_;
};

}  // namespace edge

#endif  // EDGELLM_LLM_STRUCTUREDRESULT_H
