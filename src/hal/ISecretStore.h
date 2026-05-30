// EdgeLLM — secret storage abstraction.
// Arduino-independent interface. Secrets (API keys, bearer tokens, WiFi
// passwords) are read and written through this interface so the rest of the
// library never touches platform flash APIs directly and can be tested with an
// in-memory implementation.
//
// Security: implementations must never log values. Keys are plain identifiers
// (e.g. "anthropic_api_key"); values are sensitive.
#ifndef EDGELLM_HAL_ISECRETSTORE_H
#define EDGELLM_HAL_ISECRETSTORE_H

#include <string>

#include "../core/Result.h"

namespace edge {

class ISecretStore {
 public:
  virtual ~ISecretStore() = default;

  // Stores (or overwrites) a secret. Returns a storage error on failure.
  virtual Status set(const std::string& key, const std::string& value) = 0;

  // Retrieves a secret. Returns Error::SecretNotFound if the key is absent.
  virtual Result<std::string> get(const std::string& key) = 0;

  virtual bool has(const std::string& key) = 0;

  // Removes a secret. Succeeds (no-op) if the key is absent.
  virtual Status remove(const std::string& key) = 0;

  // Removes every secret this store owns. Use with care.
  virtual Status clear() = 0;
};

}  // namespace edge

#endif  // EDGELLM_HAL_ISECRETSTORE_H
