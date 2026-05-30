// EdgeLLM — provisioning command processor (optional, off by default).
// Arduino-independent and pure: it turns text commands into writes against an
// ISecretStore, so WiFi credentials and API keys can be set without recompiling.
// A frontend (SerialProvisioner on-device, or a future captive portal) feeds it
// lines and prints the responses.
//
// Security: only fields explicitly declared via addField() can be written
// (prevents arbitrary key injection), and secret values are never echoed back.
#ifndef EDGELLM_PROVISIONING_PROVISIONINGSERVICE_H
#define EDGELLM_PROVISIONING_PROVISIONINGSERVICE_H

#include <string>
#include <vector>

#include "../hal/ISecretStore.h"

namespace edge {

class ProvisioningService {
 public:
  explicit ProvisioningService(ISecretStore& store) : store_(store) {}

  // Declares a provisionable field. `secret` controls whether the value is
  // treated as sensitive (never echoed). Returns *this for chaining.
  ProvisioningService& addField(const std::string& key, const std::string& description,
                                bool secret = true);

  // Processes one command line and returns a human-readable response.
  // Commands: help | set <key> <value> | status | clear <key> | done
  std::string handleCommand(const std::string& line);

 private:
  struct Field {
    std::string key;
    std::string description;
    bool secret;
  };
  const Field* findField(const std::string& key) const;

  ISecretStore& store_;
  std::vector<Field> fields_;
};

}  // namespace edge

#endif  // EDGELLM_PROVISIONING_PROVISIONINGSERVICE_H
