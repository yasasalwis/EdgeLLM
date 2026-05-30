// EdgeLLM — RAM-backed secret store.
// Arduino-independent. Non-persistent: contents vanish on reboot. Used as the
// store in native tests, and as the fallback on boards without a persistent
// flash KV (e.g. some SAMD/WiFiNINA setups) where the sketch supplies secrets
// at startup.
#ifndef EDGELLM_HAL_MEMORYSECRETSTORE_H
#define EDGELLM_HAL_MEMORYSECRETSTORE_H

#include <map>
#include <string>

#include "ISecretStore.h"

namespace edge {

class MemorySecretStore : public ISecretStore {
 public:
  Status set(const std::string& key, const std::string& value) override;
  Result<std::string> get(const std::string& key) override;
  bool has(const std::string& key) override;
  Status remove(const std::string& key) override;
  Status clear() override;

  size_t size() const { return items_.size(); }

 private:
  std::map<std::string, std::string> items_;
};

}  // namespace edge

#endif  // EDGELLM_HAL_MEMORYSECRETSTORE_H
