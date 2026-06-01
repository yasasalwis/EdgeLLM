#include "EepromSecretStore.h"

#if defined(EDGELLM_PLATFORM_UNO_R4) || defined(EDGELLM_PLATFORM_SAMD)

#if defined(EDGELLM_PLATFORM_SAMD)
// FlashStorage library provides an EEPROM-compatible object that must be
// commit()ed to persist.
#include <FlashAsEEPROM.h>
#define EDGELLM_EEPROM_NEEDS_COMMIT 1
#else
// Uno R4 (Renesas) ships a built-in EEPROM library backed by on-chip data flash;
// writes persist without an explicit commit.
#include <EEPROM.h>
#endif

namespace edge {

size_t EepromSecretStore::capacityBytes() const {
  const size_t hw = static_cast<size_t>(EEPROM.length());
  if (capacity_ == 0) return hw;
  return capacity_ < hw ? capacity_ : hw;
}

void EepromSecretStore::writeBlob(const std::vector<uint8_t>& blob) {
  for (size_t i = 0; i < blob.size(); ++i) {
    EEPROM.write(static_cast<int>(i), blob[i]);
  }
#if defined(EDGELLM_EEPROM_NEEDS_COMMIT)
  EEPROM.commit();
#endif
}

void EepromSecretStore::begin() {
  const size_t n = capacityBytes();
  std::vector<uint8_t> blob;
  blob.reserve(n);
  for (size_t i = 0; i < n; ++i)
    blob.push_back(EEPROM.read(static_cast<int>(i)));
  // Invalid/empty EEPROM -> data_ stays empty (decodeKv clears on bad magic).
  decodeKv(blob.data(), blob.size(), data_);
}

Status EepromSecretStore::set(const std::string& key, const std::string& value) {
  if (key.empty()) return Status::fail(Error::InvalidArgument);
  std::map<std::string, std::string> candidate = data_;
  candidate[key] = value;
  std::vector<uint8_t> blob;
  if (!encodeKv(candidate, blob, capacityBytes())) return Status::fail(Error::Capacity);
  data_.swap(candidate);
  writeBlob(blob);
  return Status::ok();
}

Result<std::string> EepromSecretStore::get(const std::string& key) {
  auto it = data_.find(key);
  if (it == data_.end()) return Result<std::string>::fail(Error::SecretNotFound);
  return Result<std::string>::ok(it->second);
}

bool EepromSecretStore::has(const std::string& key) { return data_.find(key) != data_.end(); }

Status EepromSecretStore::remove(const std::string& key) {
  if (data_.find(key) == data_.end()) return Status::ok();  // idempotent
  std::map<std::string, std::string> candidate = data_;
  candidate.erase(key);
  std::vector<uint8_t> blob;
  if (!encodeKv(candidate, blob, capacityBytes())) return Status::fail(Error::Capacity);
  data_.swap(candidate);
  writeBlob(blob);
  return Status::ok();
}

Status EepromSecretStore::clear() {
  data_.clear();
  std::vector<uint8_t> blob;
  encodeKv(data_, blob, capacityBytes());  // empty store always fits
  writeBlob(blob);
  return Status::ok();
}

}  // namespace edge

#endif  // UNO_R4 || SAMD
