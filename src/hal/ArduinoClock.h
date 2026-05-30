// EdgeLLM — millis() adapter for HttpClient's injectable clock.
// Guarded to Arduino builds. Pass edgeArduinoMillis to HttpClient::setClock so
// request timeouts are enforced against the real monotonic clock.
#ifndef EDGELLM_HAL_ARDUINOCLOCK_H
#define EDGELLM_HAL_ARDUINOCLOCK_H

#include "Platform.h"

#if defined(EDGELLM_HAS_ARDUINO)

#include <Arduino.h>

namespace edge {

inline uint32_t edgeArduinoMillis() { return static_cast<uint32_t>(millis()); }

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
#endif  // EDGELLM_HAL_ARDUINOCLOCK_H
