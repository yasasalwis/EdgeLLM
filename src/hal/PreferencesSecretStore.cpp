#include "PreferencesSecretStore.h"

#if defined(EDGELLM_PLATFORM_ESP32)

namespace edge {

namespace {
// FNV-1a 32-bit hash — small, fast, good enough for key derivation (not for
// security; values themselves are stored as-is in NVS).
uint32_t fnv1a(const std::string& s) {
  uint32_t h = 2166136261u;
  for (char c : s) {
    h ^= static_cast<uint8_t>(c);
    h *= 16777619u;
  }
  return h;
}
}  // namespace

PreferencesSecretStore::PreferencesSecretStore(const char* nvsNamespace) : ns_(nvsNamespace) {}

std::string PreferencesSecretStore::shortKey(const std::string& key) const {
  // "k" + 8 hex chars = 9 chars, comfortably under the 15-char NVS limit.
  const uint32_t h = fnv1a(key);
  char buf[10];
  static const char* hex = "0123456789abcdef";
  buf[0] = 'k';
  for (int i = 0; i < 8; ++i)
    buf[1 + i] = hex[(h >> ((7 - i) * 4)) & 0xF];
  buf[9] = '\0';
  return std::string(buf);
}

Status PreferencesSecretStore::set(const std::string& key, const std::string& value) {
  if (key.empty()) return Status::fail(Error::InvalidArgument);
  Preferences prefs;
  if (!prefs.begin(ns_, /*readOnly=*/false)) return Status::fail(Error::SecretStoreError);
  const size_t written = prefs.putString(shortKey(key).c_str(), value.c_str());
  prefs.end();
  // putString returns 0 on failure for a non-empty value.
  if (written == 0 && !value.empty()) return Status::fail(Error::SecretStoreError);
  return Status::ok();
}

Result<std::string> PreferencesSecretStore::get(const std::string& key) {
  Preferences prefs;
  if (!prefs.begin(ns_, /*readOnly=*/true))
    return Result<std::string>::fail(Error::SecretStoreError);
  const std::string sk = shortKey(key);
  if (!prefs.isKey(sk.c_str())) {
    prefs.end();
    return Result<std::string>::fail(Error::SecretNotFound);
  }
  String value = prefs.getString(sk.c_str(), "");
  prefs.end();
  return Result<std::string>::ok(std::string(value.c_str()));
}

bool PreferencesSecretStore::has(const std::string& key) {
  Preferences prefs;
  if (!prefs.begin(ns_, /*readOnly=*/true)) return false;
  const bool present = prefs.isKey(shortKey(key).c_str());
  prefs.end();
  return present;
}

Status PreferencesSecretStore::remove(const std::string& key) {
  Preferences prefs;
  if (!prefs.begin(ns_, /*readOnly=*/false)) return Status::fail(Error::SecretStoreError);
  prefs.remove(shortKey(key).c_str());
  prefs.end();
  return Status::ok();
}

Status PreferencesSecretStore::clear() {
  Preferences prefs;
  if (!prefs.begin(ns_, /*readOnly=*/false)) return Status::fail(Error::SecretStoreError);
  prefs.clear();
  prefs.end();
  return Status::ok();
}

}  // namespace edge

#endif  // EDGELLM_PLATFORM_ESP32
