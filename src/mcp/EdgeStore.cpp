#include "EdgeStore.h"

namespace edge {

namespace {
// Reserved backend key holding the list of stored keys (NVS can't enumerate).
// File-local to avoid any static-member linkage concerns across Arduino cores.
constexpr char kManifestKey[] = "__edgestore_manifest__";

std::vector<std::string> splitLines(const std::string& s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    size_t nl = s.find('\n', start);
    if (nl == std::string::npos) {
      if (start < s.size()) out.push_back(s.substr(start));
      break;
    }
    if (nl > start) out.push_back(s.substr(start, nl - start));
    start = nl + 1;
  }
  return out;
}
}  // namespace

void EdgeStore::setPersistence(ISecretStore* backend) {
  backend_ = backend;
  if (backend_ == nullptr) return;
  // Load the manifest, then each value, into RAM. No change events: this is
  // initial state, not a change.
  Result<std::string> manifest = backend_->get(kManifestKey);
  if (!manifest.isOk()) return;
  for (const auto& key : splitLines(manifest.value())) {
    Result<std::string> v = backend_->get(key);
    if (v.isOk() && data_.size() < maxKeys_) data_[key] = v.value();
  }
}

void EdgeStore::persistManifest() {
  if (backend_ == nullptr) return;
  // Only durable entries belong in the manifest — TTL entries are RAM-only.
  std::string joined;
  for (const auto& kv : data_) {
    if (expiry_.find(kv.first) != expiry_.end()) continue;
    if (!joined.empty()) joined += '\n';
    joined += kv.first;
  }
  backend_->set(kManifestKey, joined);
}

bool EdgeStore::expiredNow(const std::string& key) const {
  if (clock_ == nullptr) return false;
  auto it = expiry_.find(key);
  if (it == expiry_.end()) return false;
  // Wrap-safe comparison on the 32-bit millisecond clock.
  return static_cast<int32_t>(clock_() - it->second) >= 0;
}

void EdgeStore::evictExpired() {
  if (clock_ == nullptr || expiry_.empty()) return;
  std::vector<std::string> dead;
  for (const auto& kv : expiry_) {
    if (static_cast<int32_t>(clock_() - kv.second) >= 0) dead.push_back(kv.first);
  }
  for (const auto& key : dead) {
    data_.erase(key);
    expiry_.erase(key);
    if (onChange_) onChange_(key, /*removed=*/true);
  }
}

Status EdgeStore::validateAndStore(const std::string& key, const std::string& value) {
  if (key.empty() || key.size() > maxKeyLen_) return Status::fail(Error::InvalidArgument);
  if (key.find('\n') != std::string::npos) return Status::fail(Error::InvalidArgument);
  if (key == kManifestKey) return Status::fail(Error::InvalidArgument);  // reserved
  if (value.size() > maxValueLen_) return Status::fail(Error::Capacity);
  const bool isNew = data_.find(key) == data_.end();
  if (isNew && data_.size() >= maxKeys_) return Status::fail(Error::Capacity);
  data_[key] = value;
  return Status::ok();
}

Status EdgeStore::set(const std::string& key, const std::string& value) {
  evictExpired();
  const bool wasTtl = expiry_.find(key) != expiry_.end();
  const bool isNew = data_.find(key) == data_.end();
  Status stored = validateAndStore(key, value);
  if (!stored.isOk()) return stored;
  expiry_.erase(key);  // a plain set makes the entry durable
  if (backend_ != nullptr) {
    Status s = backend_->set(key, value);
    if (!s.isOk()) return s;
    // Rewrite the manifest only when membership changed (flash-wear friendly):
    // a brand-new key, or a TTL entry promoted to durable.
    if (isNew || wasTtl) persistManifest();
  }
  if (onChange_) onChange_(key, /*removed=*/false);
  return Status::ok();
}

Status EdgeStore::setWithTtl(const std::string& key, const std::string& value, uint32_t ttlMs) {
  if (clock_ == nullptr) return Status::fail(Error::InvalidState);  // TTL needs a clock
  evictExpired();
  const bool wasDurable = data_.find(key) != data_.end() && expiry_.find(key) == expiry_.end();
  Status stored = validateAndStore(key, value);
  if (!stored.isOk()) return stored;
  expiry_[key] = clock_() + ttlMs;
  if (wasDurable && backend_ != nullptr) {
    // The persisted copy would resurrect a stale value on reboot — drop it.
    backend_->remove(key);
    persistManifest();
  }
  if (onChange_) onChange_(key, /*removed=*/false);
  return Status::ok();
}

Result<std::string> EdgeStore::get(const std::string& key) {
  evictExpired();
  auto it = data_.find(key);
  if (it == data_.end()) return Result<std::string>::fail(Error::NotFound);
  return Result<std::string>::ok(it->second);
}

Status EdgeStore::remove(const std::string& key) {
  if (key == kManifestKey) return Status::ok();  // never touch the reserved manifest
  evictExpired();
  auto it = data_.find(key);
  if (it == data_.end()) return Status::ok();  // idempotent
  data_.erase(it);
  expiry_.erase(key);
  if (backend_ != nullptr) {
    backend_->remove(key);
    persistManifest();
  }
  if (onChange_) onChange_(key, /*removed=*/true);
  return Status::ok();
}

std::vector<std::string> EdgeStore::keys() const {
  std::vector<std::string> out;
  out.reserve(data_.size());
  for (const auto& kv : data_) {
    if (!expiredNow(kv.first)) out.push_back(kv.first);
  }
  return out;
}

size_t EdgeStore::size() const {
  size_t n = 0;
  for (const auto& kv : data_) {
    if (!expiredNow(kv.first)) ++n;
  }
  return n;
}

}  // namespace edge
