// EdgeLLM — Example 04: Streaming Chat
//
// Streams a reply token-by-token and prints each fragment as it arrives — the
// low-RAM path, since the full reply is never buffered by the transport.
// Target: ESP32. Uses OpenAI here; any SSE provider streams the same way.
//
// Prereqs: ArduinoJson installed, `tools/gen_ca_bundle.py` run, and
// arduino_secrets.h filled in.

#include <EdgeLLM.h>

#include "arduino_secrets.h"

edge::Logger logger;
edge::SerialLogSink serialSink;
edge::NtpTimeSource ntp;

edge::OpenAIProvider provider(OPENAI_API_KEY);
edge::ArduinoSecureConnection conn;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  logger.setSink(&serialSink);
  logger.registerSecret(WIFI_PASSWORD);
  logger.registerSecret(OPENAI_API_KEY);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(250);
  if (WiFi.status() != WL_CONNECTED) {
    logger.error("WiFi failed");
    return;
  }
  ntp.begin();

#if EDGELLM_HAS_CA_BUNDLE
  conn.trust().useDefaultBundle();
#else
  logger.warn("No CA bundle — run tools/gen_ca_bundle.py. Refusing insecure TLS.");
  return;
#endif

  edge::LLMClient client(provider, conn, &logger);
  client.setClock(edge::edgeArduinoMillis);
  client.options().maxTokens = 200;

  Serial.println("\n--- streaming reply ---");
  edge::ChatResult final;
  edge::Status s = client.chatStream(
      "Write a short haiku about embedded systems.",
      [](const std::string& delta) { Serial.print(delta.c_str()); },  // print as it streams
      &final);
  Serial.println("\n--- end ---");

  if (!s.isOk()) {
    logger.error(std::string("stream failed: ") + s.message());
    return;
  }
  logger.info(std::string("output tokens: ") + String(final.outputTokens).c_str());
}

void loop() { delay(1000); }
