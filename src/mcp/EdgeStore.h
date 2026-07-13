// EdgeLLM — built-in key/value datastore exposed over MCP.
// Arduino-independent. RAM-backed with bounded size, and optional persistence
// through any ISecretStore (NVS on ESP32). The MCP server auto-exposes the
// contents as readable resources (kv://<key>) and, when writes are enabled,
// kv_set / kv_delete tools — so an LLM host can read and write device data with
// zero handler code (the "both: handlers + built-in KV store" decision).
//
// Optional extras: a change hook (react when a host writes a key) and TTL
// entries (values that expire, e.g. a cached reading — RAM-only, lazily
// evicted, require a clock).
#ifndef EDGELLM_MCP_EDGESTORE_H
#define EDGELLM_MCP_EDGESTORE_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "../core/Result.h"
#include "../hal/ISecretStore.h"

namespace edge {

class EdgeStore {
 public:
  // Fired after a key is stored/updated (removed=false) or removed/expired
  // (removed=true). Lets a sketch react when an MCP host writes device data.
  using ChangeFn = std::function<void(const std::string& key, bool removed)>;
  // Millisecond monotonic clock (edgeArduinoMillis on device); enables TTL.
  using ClockFn = uint32_t (*)();

  // Bounds guard RAM and flash: at most `maxKeys` entries, each value at most
  // `maxValueLen` bytes, each key at most `maxKeyLen` bytes.
  EdgeStore(size_t maxKeys = 32, size_t maxValueLen = 512, size_t maxKeyLen = 64)
      : maxKeys_(maxKeys), maxValueLen_(maxValueLen), maxKeyLen_(maxKeyLen) {}

  // Attaches a persistence backend and loads any previously persisted entries.
  // The backend stores the values plus a manifest key listing them (NVS cannot
  // enumerate keys on its own). Pass nullptr for RAM-only.
  void setPersistence(ISecretStore* backend);

  void setOnChange(ChangeFn fn) { onChange_ = std::move(fn); }
  void setClock(ClockFn clock) { clock_ = clock; }

  // Stores/overwrites a value. Enforces the key/value/count caps.
  Status set(const std::string& key, const std::string& value);

  // Stores a value that expires `ttlMs` from now (lazily evicted). Requires a
  // clock (InvalidState otherwise). TTL entries are RAM-only: they are never
  // written to the persistence backend, and overwriting a persisted key with a
  // TTL entry removes the persisted copy.
  Status setWithTtl(const std::string& key, const std::string& value, uint32_t ttlMs);

  Result<std::string> get(const std::string& key);
  bool has(const std::string& key) const {
    auto it = data_.find(key);
    return it != data_.end() && !expiredNow(key);
  }
  Status remove(const std::string& key);

  // Live (non-expired) keys / count.
  std::vector<std::string> keys() const;
  size_t size() const;

 private:
  void persistManifest();
  bool expiredNow(const std::string& key) const;
  void evictExpired();
  Status validateAndStore(const std::string& key, const std::string& value);

  std::map<std::string, std::string> data_;
  std::map<std::string, uint32_t> expiry_;  // key -> absolute deadline (clock ms)
  size_t maxKeys_;
  size_t maxValueLen_;
  size_t maxKeyLen_;
  ISecretStore* backend_ = nullptr;
  ChangeFn onChange_;
  ClockFn clock_ = nullptr;
};

}  // namespace edge

#endif  // EDGELLM_MCP_EDGESTORE_H
