// EdgeLLM — EEPROM-backed persistent secret store for non-ESP32 boards.
// Persists secrets/KV across reboots on boards that expose an Arduino EEPROM
// object: Uno R4 WiFi (built-in EEPROM) and SAMD / Nano 33 IoT (via the
// FlashStorage library's EEPROM emulation). ESP32 should use the NVS-backed
// PreferencesSecretStore instead.
//
// The whole store is serialized as one blob (see EepromCodec) and rewritten on
// each mutation — fine for the small, infrequently-written secret set this is
// meant for. Usable directly for secrets, or as an EdgeStore persistence backend.
#ifndef EDGELLM_HAL_EEPROMSECRETSTORE_H
#define EDGELLM_HAL_EEPROMSECRETSTORE_H

#include "Platform.h"

#if defined(EDGELLM_PLATFORM_UNO_R4) || defined(EDGELLM_PLATFORM_SAMD)

#include <map>
#include <string>

#include "EepromCodec.h"
#include "ISecretStore.h"

namespace edge {

class EepromSecretStore : public ISecretStore {
 public:
  // `capacity` caps how many bytes of EEPROM the store may use; 0 means use the
  // full EEPROM.length().
  explicit EepromSecretStore(size_t capacity = 0) : capacity_(capacity) {}

  // Loads any previously persisted entries from EEPROM. Call once in setup().
  void begin();

  Status set(const std::string& key, const std::string& value) override;
  Result<std::string> get(const std::string& key) override;
  bool has(const std::string& key) override;
  Status remove(const std::string& key) override;
  Status clear() override;

  size_t size() const { return data_.size(); }

 private:
  size_t capacityBytes() const;
  void writeBlob(const std::vector<uint8_t>& blob);

  std::map<std::string, std::string> data_;
  size_t capacity_;
};

}  // namespace edge

#endif  // UNO_R4 || SAMD
#endif  // EDGELLM_HAL_EEPROMSECRETSTORE_H
