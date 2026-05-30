#include "NtpTimeSource.h"

#if defined(EDGELLM_HAS_ARDUINO)

#include <Arduino.h>

#if defined(EDGELLM_PLATFORM_ESP32) || defined(EDGELLM_PLATFORM_ESP8266)
#include <time.h>
#elif defined(EDGELLM_PLATFORM_UNO_R4)
#include <WiFiS3.h>
#elif defined(EDGELLM_PLATFORM_SAMD)
#include <WiFiNINA.h>
#endif

namespace edge {

bool NtpTimeSource::begin(uint32_t timeoutMs, const char* server1, const char* server2) {
#if defined(EDGELLM_PLATFORM_ESP32) || defined(EDGELLM_PLATFORM_ESP8266)
  // UTC, no DST offset; certificate checks operate in UTC.
  configTime(0, 0, server1, server2);
  const uint32_t start = millis();
  while (static_cast<uint32_t>(time(nullptr)) < kPlausibleEpochFloor) {
    if (millis() - start > timeoutMs) return false;
    delay(100);
  }
  return true;
#elif defined(EDGELLM_PLATFORM_UNO_R4) || defined(EDGELLM_PLATFORM_SAMD)
  (void)server1;
  (void)server2;
  const uint32_t start = millis();
  while (epoch() < kPlausibleEpochFloor) {
    if (millis() - start > timeoutMs) return false;
    delay(200);
  }
  return true;
#else
  (void)server1;
  (void)server2;
  (void)timeoutMs;
  return false;  // unsupported core; sketch must supply time another way
#endif
}

uint32_t NtpTimeSource::epoch() {
#if defined(EDGELLM_PLATFORM_ESP32) || defined(EDGELLM_PLATFORM_ESP8266)
  return static_cast<uint32_t>(time(nullptr));
#elif defined(EDGELLM_PLATFORM_UNO_R4) || defined(EDGELLM_PLATFORM_SAMD)
  return static_cast<uint32_t>(WiFi.getTime());
#else
  return 0;
#endif
}

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
