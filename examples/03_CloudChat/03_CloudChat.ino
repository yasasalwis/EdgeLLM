// EdgeLLM — Example 03: Cloud Chat (blocking)
//
// Sends one prompt to Claude and prints the reply. Target: ESP32.
//
// Prereqs:
//   1) Install ArduinoJson via the Library Manager.
//   2) Run `python3 tools/gen_ca_bundle.py` once to enable verified TLS.
//   3) Copy arduino_secrets.h.example -> arduino_secrets.h and fill it in.
//
// Swap AnthropicProvider for OpenAIProvider / GeminiProvider to use a different
// backend — the rest of the sketch is identical.

#include <EdgeLLM.h>

#include "arduino_secrets.h"

edge::Logger logger;
edge::SerialLogSink serialSink;
edge::NtpTimeSource ntp;

edge::AnthropicProvider provider(ANTHROPIC_API_KEY);
edge::ArduinoSecureConnection conn;

namespace {
void connectWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(250);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  logger.setSink(&serialSink);
  logger.setLevel(edge::LogLevel::Info);
  logger.registerSecret(WIFI_PASSWORD);
  logger.registerSecret(ANTHROPIC_API_KEY);  // masked if it ever hits a log

  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) {
    logger.error("WiFi failed");
    return;
  }
  if (!ntp.begin()) logger.warn("NTP sync failed; TLS may reject the cert");

#if EDGELLM_HAS_CA_BUNDLE
  conn.trust().useDefaultBundle();
#else
  logger.warn("No CA bundle — run tools/gen_ca_bundle.py. Refusing insecure TLS.");
  return;
#endif

  edge::LLMClient client(provider, conn, &logger);
  client.setClock(edge::edgeArduinoMillis);
  client.setTimeout(20000);
  client.options().system = "You are a concise assistant for a microcontroller.";
  client.options().maxTokens = 200;

  logger.info("asking Claude...");
  edge::Result<edge::ChatResult> r = client.chat("In one sentence, what is an Arduino?");
  if (!r.isOk()) {
    logger.error(std::string("chat failed: ") + r.message());
    return;
  }
  logger.info(std::string("reply: ") + r.value().text);
  logger.info(std::string("tokens in/out: ") + String(r.value().inputTokens).c_str() + "/" +
              String(r.value().outputTokens).c_str());
}

void loop() { delay(1000); }
