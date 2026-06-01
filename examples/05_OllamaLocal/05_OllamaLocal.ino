// EdgeLLM — Example 05: Local Ollama, structured output (no cloud, no key)
//
// Gets a schema-validated JSON object from an Ollama server on your LAN over
// plain HTTP. Fully local: no cloud, no TLS, no key. Works on any WiFi board.
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

  edge::ResponseSchema schema("fact");
  schema.field("subject", edge::ParamType::String, "what the fact is about")
      .field("fact", edge::ParamType::String, "one interesting fact")
      .field("surprising", edge::ParamType::Boolean, "is it surprising?");

  edge::Result<edge::StructuredResult> r =
      client.generate(schema, "You return one concise fact as JSON.", "Tell me about microcontrollers.");
  if (!r.isOk()) {
    logger.error(std::string("ollama generate failed: ") + r.message());
    return;
  }
  logger.info(std::string("subject: ") + r.value().getString("subject"));
  logger.info(std::string("fact:    ") + r.value().getString("fact"));
}

void loop() { delay(1000); }
