// EdgeLLM — cumulative usage accounting with optional hard caps.
// Arduino-independent. An always-on device calling a paid API needs a spend
// guard: attach a UsageMeter to the LLMClient and every HTTP request checks the
// caps first — when one is exhausted, calls fail fast with
// Error::BudgetExceeded instead of spending more tokens.
//
// Counters are RAM-only and reset on reboot; a sketch that wants a daily cap
// can call reset() from its own scheduler, or persist counters via any
// ISecretStore if they must survive deep sleep.
#ifndef EDGELLM_LLM_USAGEMETER_H
#define EDGELLM_LLM_USAGEMETER_H

#include <cstdint>

namespace edge {

class UsageMeter {
 public:
  // Caps; 0 means unlimited (the default).
  void setMaxRequests(uint32_t n) { maxRequests_ = n; }
  void setMaxTotalTokens(uint32_t n) { maxTotalTokens_ = n; }

  // True while no cap is exhausted. Checked by LLMClient before every request.
  bool allowRequest() const {
    if (maxRequests_ != 0 && requests_ >= maxRequests_) return false;
    if (maxTotalTokens_ != 0 && inputTokens_ + outputTokens_ >= maxTotalTokens_) return false;
    return true;
  }

  void recordRequest() { ++requests_; }
  void recordTokens(uint32_t input, uint32_t output) {
    inputTokens_ += input;
    outputTokens_ += output;
  }

  uint32_t requests() const { return requests_; }
  uint32_t inputTokens() const { return inputTokens_; }
  uint32_t outputTokens() const { return outputTokens_; }
  uint32_t totalTokens() const { return inputTokens_ + outputTokens_; }

  // Starts a fresh accounting period (caps stay configured).
  void reset() {
    requests_ = 0;
    inputTokens_ = 0;
    outputTokens_ = 0;
  }

 private:
  uint32_t maxRequests_ = 0;
  uint32_t maxTotalTokens_ = 0;
  uint32_t requests_ = 0;
  uint32_t inputTokens_ = 0;
  uint32_t outputTokens_ = 0;
};

}  // namespace edge

#endif  // EDGELLM_LLM_USAGEMETER_H
