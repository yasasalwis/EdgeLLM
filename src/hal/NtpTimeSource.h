// EdgeLLM — NTP-backed time source for Arduino targets.
// Guarded to Arduino builds. Synchronizes the system clock over SNTP so TLS
// certificate validity can be checked. Call begin() once after WiFi is up.
#ifndef EDGELLM_HAL_NTPTIMESOURCE_H
#define EDGELLM_HAL_NTPTIMESOURCE_H

#include "Platform.h"

#if defined(EDGELLM_HAS_ARDUINO)

#include "ITimeSource.h"

namespace edge {

class NtpTimeSource : public ITimeSource {
 public:
  // Triggers SNTP synchronization and blocks until a plausible time is obtained
  // or `timeoutMs` elapses. Returns true on success. Safe to call again to
  // re-sync. Requires an active network connection.
  bool begin(uint32_t timeoutMs = 8000, const char* server1 = "pool.ntp.org",
             const char* server2 = "time.nist.gov");

  uint32_t epoch() override;
  bool isValid() override { return epoch() >= kPlausibleEpochFloor; }
};

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
#endif  // EDGELLM_HAL_NTPTIMESOURCE_H
