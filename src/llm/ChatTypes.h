// EdgeLLM — chat request options and result types.
// Arduino-independent.
#ifndef EDGELLM_LLM_CHATTYPES_H
#define EDGELLM_LLM_CHATTYPES_H

#include <cstdint>
#include <string>

namespace edge {

// Sentinel meaning "let the provider use its own default" for optional numeric
// knobs, so we only send what the caller explicitly set.
constexpr float kUnsetTemperature = -1.0f;

struct ChatOptions {
  std::string model;   // provider model id; empty -> provider's default
  std::string system;  // system prompt; empty -> none
  uint32_t maxTokens = 1024;
  float temperature = kUnsetTemperature;  // [0,2] typical; unset -> not sent

  bool hasTemperature() const { return temperature >= 0.0f; }
};

// Response metadata accompanying a structured generation (the JSON payload is
// returned separately as a StructuredResult).
struct ChatResult {
  std::string finishReason;  // provider-specific, e.g. "stop", "end_turn", "length"
  uint32_t inputTokens = 0;  // usage if the provider reports it
  uint32_t outputTokens = 0;
};

}  // namespace edge

#endif  // EDGELLM_LLM_CHATTYPES_H
