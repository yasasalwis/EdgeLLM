// EdgeLLM — millis()/delay() adapters for the library's injectable clock and
// delay hooks. Guarded to Arduino builds. Pass edgeArduinoMillis to
// HttpClient/LLMClient::setClock so timeouts run against the real monotonic
// clock, and edgeArduinoDelay to LLMClient::setDelayFn so retry backoff yields
// to the core's scheduler instead of busy-waiting.
#ifndef EDGELLM_HAL_ARDUINOCLOCK_H
#define EDGELLM_HAL_ARDUINOCLOCK_H

#include <cstdint>

#include "Platform.h"

#if defined(EDGELLM_HAS_ARDUINO)

#include <Arduino.h>

namespace edge {

inline uint32_t edgeArduinoMillis() { return static_cast<uint32_t>(millis()); }
inline void edgeArduinoDelay(uint32_t ms) { delay(ms); }

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
#endif  // EDGELLM_HAL_ARDUINOCLOCK_H
