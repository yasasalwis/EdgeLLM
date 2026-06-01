// EdgeLLM — Serial frontend for ProvisioningService (device-only, guarded).
// Reads newline-terminated commands from a Stream (Serial by default) and prints
// the service responses. The protocol logic lives in ProvisioningService (which
// is unit-tested); this is just line assembly + I/O.
#ifndef EDGELLM_PROVISIONING_SERIALPROVISIONER_H
#define EDGELLM_PROVISIONING_SERIALPROVISIONER_H

#include "../hal/Platform.h"

#if defined(EDGELLM_HAS_ARDUINO)

#include <Arduino.h>

#include "ProvisioningService.h"

namespace edge {

class SerialProvisioner {
 public:
  explicit SerialProvisioner(ProvisioningService& service, Stream& io = Serial)
      : service_(service), io_(io) {}

  void begin() { io_.println("EdgeLLM provisioning ready. Type 'help'."); }

  // Call from loop(). Assembles complete lines and dispatches them.
  void poll() {
    while (io_.available() > 0) {
      const int c = io_.read();
      if (c < 0) break;
      if (c == '\n') {
        const std::string response = service_.handleCommand(buf_);
        buf_.clear();
        if (!response.empty()) io_.println(response.c_str());
      } else if (c != '\r') {
        if (buf_.size() < kMaxLine) buf_.push_back(static_cast<char>(c));
      }
    }
  }

 private:
  static constexpr size_t kMaxLine = 512;
  ProvisioningService& service_;
  Stream& io_;
  std::string buf_;
};

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
#endif  // EDGELLM_PROVISIONING_SERIALPROVISIONER_H
