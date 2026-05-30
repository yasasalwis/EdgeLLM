// EdgeLLM — log sink that writes to the Arduino Serial port.
// Guarded to Arduino builds. Pair with a Logger: logger.setSink(&sink).
#ifndef EDGELLM_HAL_SERIALLOGSINK_H
#define EDGELLM_HAL_SERIALLOGSINK_H

#include "Platform.h"

#if defined(EDGELLM_HAS_ARDUINO)

#include <Arduino.h>

#include "../core/Logger.h"

namespace edge {

class SerialLogSink : public ILogSink {
 public:
  void write(LogLevel level, const std::string& line) override {
    (void)level;
    Serial.println(line.c_str());
  }
};

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
#endif  // EDGELLM_HAL_SERIALLOGSINK_H
