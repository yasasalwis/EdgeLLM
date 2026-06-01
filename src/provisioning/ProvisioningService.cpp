#include "ProvisioningService.h"

namespace edge {

namespace {
std::string trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
    ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
    --e;
  return s.substr(b, e - b);
}
}  // namespace

ProvisioningService& ProvisioningService::addField(const std::string& key,
                                                   const std::string& description, bool secret) {
  fields_.push_back({key, description, secret});
  return *this;
}

const ProvisioningService::Field* ProvisioningService::findField(const std::string& key) const {
  for (const auto& f : fields_) {
    if (f.key == key) return &f;
  }
  return nullptr;
}

std::string ProvisioningService::handleCommand(const std::string& lineIn) {
  const std::string line = trim(lineIn);
  if (line.empty()) return "";

  // Split into command and remainder.
  const size_t sp = line.find(' ');
  const std::string cmd = (sp == std::string::npos) ? line : line.substr(0, sp);
  const std::string rest = (sp == std::string::npos) ? "" : trim(line.substr(sp + 1));

  if (cmd == "help") {
    std::string out = "commands: help | set <key> <value> | status | clear <key> | done\nfields:";
    for (const auto& f : fields_) {
      out += "\n  " + f.key + " - " + f.description + (f.secret ? " (secret)" : "");
    }
    return out;
  }

  if (cmd == "status") {
    std::string out = "field status:";
    for (const auto& f : fields_) {
      out += "\n  " + f.key + ": " + (store_.has(f.key) ? "set" : "unset");
    }
    return out;
  }

  if (cmd == "set") {
    const size_t sp2 = rest.find(' ');
    if (sp2 == std::string::npos) return "usage: set <key> <value>";
    const std::string key = rest.substr(0, sp2);
    const std::string value = trim(rest.substr(sp2 + 1));
    if (value.empty()) return "usage: set <key> <value>";
    const Field* f = findField(key);
    if (f == nullptr) return "unknown field: " + key;
    Status s = store_.set(key, value);
    if (!s.isOk()) return std::string("error: ") + s.message();
    return "ok: " + key + " set";  // never echo the value
  }

  if (cmd == "clear") {
    if (rest.empty()) return "usage: clear <key>";
    if (findField(rest) == nullptr) return "unknown field: " + rest;
    store_.remove(rest);
    return "cleared: " + rest;
  }

  if (cmd == "done") {
    return "provisioning complete";
  }

  return "unknown command: " + cmd + " (try help)";
}

}  // namespace edge
