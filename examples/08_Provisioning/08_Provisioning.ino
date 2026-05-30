// EdgeLLM — Example 08: Serial Provisioning (set secrets without recompiling)
//
// Stores WiFi credentials and API keys into NVS over the Serial Monitor, so you
// don't have to hard-code them or re-flash to change them. Target: ESP32.
//
// Open the Serial Monitor at 115200 (line ending: Newline) and type:
//   help
//   set wifi_ssid MyNetwork
//   set wifi_password hunter2
//   set anthropic_api_key sk-ant-...
//   status
//   done
//
// Other sketches then read these via the same PreferencesSecretStore namespace.

#include <EdgeLLM.h>

edge::PreferencesSecretStore secrets("edgellm_sec");
edge::ProvisioningService provisioner(secrets);
edge::SerialProvisioner serialUi(provisioner);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }

  // Declare exactly which keys may be provisioned (nothing else can be written).
  provisioner.addField("wifi_ssid", "WiFi network name", /*secret=*/false)
      .addField("wifi_password", "WiFi password")
      .addField("anthropic_api_key", "Claude API key")
      .addField("openai_api_key", "OpenAI API key")
      .addField("gemini_api_key", "Gemini API key")
      .addField("mcp_bearer_token", "MCP server bearer token");

  serialUi.begin();
}

void loop() {
  serialUi.poll();  // reads Serial lines and applies provisioning commands
  delay(5);
}
