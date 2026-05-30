// EdgeLLM — NVS-backed secret store for ESP32.
// Persists secrets in the ESP32 NVS partition via the Arduino `Preferences`
// library, surviving reboots. Only compiled on ESP32; other boards use
// MemorySecretStore in this phase (their native flash backends arrive in a
// later phase). See the README "Per-board status" table.
//
// NVS keys are limited to 15 characters, so caller keys of any length are
// mapped to a short, collision-resistant hashed key internally. Values are
// never logged.
#ifndef EDGELLM_HAL_PREFERENCESSECRETSTORE_H
#define EDGELLM_HAL_PREFERENCESSECRETSTORE_H

#include "Platform.h"

#if defined(EDGELLM_PLATFORM_ESP32)

#include <Preferences.h>

#include "ISecretStore.h"

namespace edge {

class PreferencesSecretStore : public ISecretStore {
 public:
  // `nvsNamespace` selects the NVS namespace (<= 15 chars). Defaults to a
  // dedicated namespace so EdgeLLM secrets never collide with sketch storage.
  explicit PreferencesSecretStore(const char* nvsNamespace = "edgellm_sec");

  Status set(const std::string& key, const std::string& value) override;
  Result<std::string> get(const std::string& key) override;
  bool has(const std::string& key) override;
  Status remove(const std::string& key) override;
  Status clear() override;

 private:
  // Maps an arbitrary-length caller key to a stable <=15 char NVS key.
  std::string shortKey(const std::string& key) const;

  const char* ns_;
};

}  // namespace edge

#endif  // EDGELLM_PLATFORM_ESP32
#endif  // EDGELLM_HAL_PREFERENCESSECRETSTORE_H
