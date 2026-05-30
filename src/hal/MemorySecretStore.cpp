#include "MemorySecretStore.h"

namespace edge {

Status MemorySecretStore::set(const std::string& key, const std::string& value) {
  if (key.empty()) return Status::fail(Error::InvalidArgument);
  items_[key] = value;
  return Status::ok();
}

Result<std::string> MemorySecretStore::get(const std::string& key) {
  auto it = items_.find(key);
  if (it == items_.end()) return Result<std::string>::fail(Error::SecretNotFound);
  return Result<std::string>::ok(it->second);
}

bool MemorySecretStore::has(const std::string& key) { return items_.find(key) != items_.end(); }

Status MemorySecretStore::remove(const std::string& key) {
  items_.erase(key);
  return Status::ok();
}

Status MemorySecretStore::clear() {
  items_.clear();
  return Status::ok();
}

}  // namespace edge
