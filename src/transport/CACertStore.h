// EdgeLLM — TLS trust configuration.
// Arduino-independent value holder. Decides what the transport trusts when it
// opens a secure connection. The actual certificate is applied to the concrete
// TLS client by the HAL NetworkFactory; this class only carries the policy so
// it can be unit-tested and shared.
//
// Security posture: verification is ON by default. `setInsecure()` is an
// explicit, logged escape hatch intended only for trusted local endpoints
// (e.g. Ollama on the LAN over plain HTTP/self-signed TLS).
#ifndef EDGELLM_TRANSPORT_CACERTSTORE_H
#define EDGELLM_TRANSPORT_CACERTSTORE_H

#include "RootCABundle.h"

namespace edge {

class CACertStore {
 public:
  // Pins a specific PEM root (or bundle). The pointer must outlive the store;
  // on-device this is typically a PROGMEM/flash string literal. Turns
  // verification on and clears any insecure flag.
  void setCACert(const char* pem) {
    pem_ = pem;
    insecure_ = false;
  }

  // Disables certificate verification entirely. Use only for trusted local
  // endpoints. The transport logs a warning whenever this is active.
  void setInsecure(bool insecure = true) { insecure_ = insecure; }

  // Falls back to the generated default bundle (if one was generated).
  void useDefaultBundle() { pem_ = defaultCABundle(); }

  bool insecure() const { return insecure_; }
  const char* pem() const { return pem_; }
  bool hasCert() const { return pem_ != nullptr; }

  // True when the configuration is usable for a verifying TLS connection: either
  // verification is explicitly disabled, or a certificate is available.
  bool isUsable() const { return insecure_ || pem_ != nullptr; }

 private:
  const char* pem_ = defaultCABundle();
  bool insecure_ = false;
};

}  // namespace edge

#endif  // EDGELLM_TRANSPORT_CACERTSTORE_H
