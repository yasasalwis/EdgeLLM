#include "EdgeStore.h"

namespace edge {

namespace {
// Reserved backend key holding the list of stored keys (NVS can't enumerate).
// File-local to avoid any static-member linkage concerns across Arduino cores.
constexpr char kManifestKey[] = "__edgestore_manifest__";

// Joins keys with '\n' for the manifest; keys are validated to be newline-free
// on the way in, so this round-trips cleanly.
std::string joinKeys(const std::map<std::string, std::string>& data) {
  std::string out;
  for (const auto& kv : data) {
    if (!out.empty()) out += '\n';
    out += kv.first;
  }
  return out;
}

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
  // Load the manifest, then each value, into RAM.
  Result<std::string> manifest = backend_->get(kManifestKey);
  if (!manifest.isOk()) return;
  for (const auto& key : splitLines(manifest.value())) {
    Result<std::string> v = backend_->get(key);
    if (v.isOk() && data_.size() < maxKeys_) data_[key] = v.value();
  }
}

void EdgeStore::persistManifest() {
  if (backend_ != nullptr) backend_->set(kManifestKey, joinKeys(data_));
}

Status EdgeStore::set(const std::string& key, const std::string& value) {
  if (key.empty() || key.size() > maxKeyLen_) return Status::fail(Error::InvalidArgument);
  if (key.find('\n') != std::string::npos) return Status::fail(Error::InvalidArgument);
  if (key == kManifestKey) return Status::fail(Error::InvalidArgument);  // reserved
  if (value.size() > maxValueLen_) return Status::fail(Error::Capacity);
  const bool isNew = data_.find(key) == data_.end();
  if (isNew && data_.size() >= maxKeys_) return Status::fail(Error::Capacity);

  data_[key] = value;
  if (backend_ != nullptr) {
    Status s = backend_->set(key, value);
    if (!s.isOk()) return s;
    if (isNew) persistManifest();
  }
  return Status::ok();
}

Result<std::string> EdgeStore::get(const std::string& key) {
  auto it = data_.find(key);
  if (it == data_.end()) return Result<std::string>::fail(Error::NotFound);
  return Result<std::string>::ok(it->second);
}

Status EdgeStore::remove(const std::string& key) {
  if (key == kManifestKey) return Status::ok();  // never touch the reserved manifest
  auto it = data_.find(key);
  if (it == data_.end()) return Status::ok();  // idempotent
  data_.erase(it);
  if (backend_ != nullptr) {
    backend_->remove(key);
    persistManifest();
  }
  return Status::ok();
}

std::vector<std::string> EdgeStore::keys() const {
  std::vector<std::string> out;
  out.reserve(data_.size());
  for (const auto& kv : data_) out.push_back(kv.first);
  return out;
}

}  // namespace edge
