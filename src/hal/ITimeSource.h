// EdgeLLM — wall-clock time abstraction.
// Arduino-independent. TLS certificate validity checks require a real Unix
// time; this interface lets the library obtain it from NTP on-device or from a
// fixed value in tests.
#ifndef EDGELLM_HAL_ITIMESOURCE_H
#define EDGELLM_HAL_ITIMESOURCE_H

#include <cstdint>

namespace edge {

// A Unix epoch earlier than this almost certainly means time has not been set.
// Used to decide isValid(). 2023-11-14T22:13:20Z.
constexpr uint32_t kPlausibleEpochFloor = 1700000000u;

class ITimeSource {
 public:
  virtual ~ITimeSource() = default;

  // Current Unix time in seconds, or 0 if unknown.
  virtual uint32_t epoch() = 0;

  // True once the clock has been set to a plausible, post-floor value.
  virtual bool isValid() = 0;
};

// Test/seed implementation: holds a fixed epoch the caller controls.
class ManualTimeSource : public ITimeSource {
 public:
  explicit ManualTimeSource(uint32_t epochSeconds = 0) : epoch_(epochSeconds) {}
  void set(uint32_t epochSeconds) { epoch_ = epochSeconds; }
  uint32_t epoch() override { return epoch_; }
  bool isValid() override { return epoch_ >= kPlausibleEpochFloor; }

 private:
  uint32_t epoch_;
};

}  // namespace edge

#endif  // EDGELLM_HAL_ITIMESOURCE_H
