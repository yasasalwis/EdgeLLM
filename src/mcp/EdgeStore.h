// EdgeLLM — built-in key/value datastore exposed over MCP.
// Arduino-independent. RAM-backed with bounded size, and optional persistence
// through any ISecretStore (NVS on ESP32). The MCP server auto-exposes the
// contents as readable resources (kv://<key>) and, when writes are enabled,
// kv_set / kv_delete tools — so an LLM host can read and write device data with
// zero handler code (the "both: handlers + built-in KV store" decision).
#ifndef EDGELLM_MCP_EDGESTORE_H
#define EDGELLM_MCP_EDGESTORE_H

#include <map>
#include <string>
#include <vector>

#include "../core/Result.h"
#include "../hal/ISecretStore.h"

namespace edge {

class EdgeStore {
 public:
  // Bounds guard RAM and flash: at most `maxKeys` entries, each value at most
  // `maxValueLen` bytes, each key at most `maxKeyLen` bytes.
  EdgeStore(size_t maxKeys = 32, size_t maxValueLen = 512, size_t maxKeyLen = 64)
      : maxKeys_(maxKeys), maxValueLen_(maxValueLen), maxKeyLen_(maxKeyLen) {}

  // Attaches a persistence backend and loads any previously persisted entries.
  // The backend stores the values plus a manifest key listing them (NVS cannot
  // enumerate keys on its own). Pass nullptr for RAM-only.
  void setPersistence(ISecretStore* backend);

  // Stores/overwrites a value. Enforces the key/value/count caps.
  Status set(const std::string& key, const std::string& value);

  Result<std::string> get(const std::string& key);
  bool has(const std::string& key) const { return data_.find(key) != data_.end(); }
  Status remove(const std::string& key);

  std::vector<std::string> keys() const;
  size_t size() const { return data_.size(); }

 private:
  void persistManifest();

  std::map<std::string, std::string> data_;
  size_t maxKeys_;
  size_t maxValueLen_;
  size_t maxKeyLen_;
  ISecretStore* backend_ = nullptr;
};

}  // namespace edge

#endif  // EDGELLM_MCP_EDGESTORE_H
