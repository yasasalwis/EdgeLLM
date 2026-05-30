// EdgeLLM — Example 05: Local Ollama (no cloud, no API key)
//
// Streams a reply from an Ollama server on your LAN over plain HTTP. This is the
// fully-local path: no cloud, no TLS, no key. Target: ESP32 (works on other
// WiFi boards too since there's no TLS).
//
// Prereqs: ArduinoJson installed; an Ollama server reachable on your LAN
// (`ollama serve`, model pulled); arduino_secrets.h filled in.

#include <EdgeLLM.h>

#include "arduino_secrets.h"

edge::Logger logger;
edge::SerialLogSink serialSink;

edge::OllamaProvider provider(OLLAMA_HOST, OLLAMA_PORT, OLLAMA_MODEL, /*secure=*/false);
edge::ArduinoPlainConnection conn;  // plain TCP for a trusted local endpoint

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  logger.setSink(&serialSink);
  logger.registerSecret(WIFI_PASSWORD);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(250);
  if (WiFi.status() != WL_CONNECTED) {
    logger.error("WiFi failed");
    return;
  }

  edge::LLMClient client(provider, conn, &logger);
  client.setClock(edge::edgeArduinoMillis);
  client.setTimeout(60000);  // local models can be slower to first token
  client.options().maxTokens = 200;

  Serial.println("\n--- streaming reply (local) ---");
  edge::Status s = client.chatStream("Give me one fun fact about microcontrollers.",
                                     [](const std::string& d) { Serial.print(d.c_str()); });
  Serial.println("\n--- end ---");
  if (!s.isOk()) logger.error(std::string("ollama chat failed: ") + s.message());
}

void loop() { delay(1000); }
