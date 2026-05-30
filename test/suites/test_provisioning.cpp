// Tests for the provisioning command processor.
#include "../../src/hal/MemorySecretStore.h"
#include "../../src/provisioning/ProvisioningService.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}
ProvisioningService configured(MemorySecretStore& store) {
  ProvisioningService svc(store);
  svc.addField("wifi_ssid", "WiFi network name", false)
      .addField("wifi_password", "WiFi password")
      .addField("anthropic_api_key", "Claude API key");
  return svc;
}
}  // namespace

TEST(provisioning_set_writes_to_store) {
  MemorySecretStore store;
  ProvisioningService svc = configured(store);
  CHECK(contains(svc.handleCommand("set wifi_ssid MyNetwork"), "ok"));
  CHECK_STR_EQ(store.get("wifi_ssid").value(), "MyNetwork");
}

TEST(provisioning_status_shows_set_and_unset) {
  MemorySecretStore store;
  ProvisioningService svc = configured(store);
  svc.handleCommand("set wifi_ssid Net");
  const std::string st = svc.handleCommand("status");
  CHECK(contains(st, "wifi_ssid: set"));
  CHECK(contains(st, "anthropic_api_key: unset"));
}

TEST(provisioning_rejects_undeclared_field) {
  MemorySecretStore store;
  ProvisioningService svc = configured(store);
  CHECK(contains(svc.handleCommand("set rogue_key value"), "unknown field"));
  CHECK(!store.has("rogue_key"));
}

TEST(provisioning_never_echoes_secret_value) {
  MemorySecretStore store;
  ProvisioningService svc = configured(store);
  const std::string resp = svc.handleCommand("set anthropic_api_key sk-ant-supersecret");
  CHECK(!contains(resp, "supersecret"));  // value must not appear in the response
  CHECK_STR_EQ(store.get("anthropic_api_key").value(), "sk-ant-supersecret");
}

TEST(provisioning_value_may_contain_spaces) {
  MemorySecretStore store;
  ProvisioningService svc = configured(store);
  svc.handleCommand("set wifi_password my pass word");
  CHECK_STR_EQ(store.get("wifi_password").value(), "my pass word");
}

TEST(provisioning_clear_removes_value) {
  MemorySecretStore store;
  ProvisioningService svc = configured(store);
  svc.handleCommand("set wifi_ssid Net");
  CHECK(contains(svc.handleCommand("clear wifi_ssid"), "cleared"));
  CHECK(!store.has("wifi_ssid"));
}

TEST(provisioning_usage_and_unknown_command) {
  MemorySecretStore store;
  ProvisioningService svc = configured(store);
  CHECK(contains(svc.handleCommand("set wifi_ssid"), "usage"));
  CHECK(contains(svc.handleCommand("clear"), "usage"));
  CHECK(contains(svc.handleCommand("frobnicate"), "unknown command"));
  CHECK(contains(svc.handleCommand("help"), "wifi_ssid"));
}
