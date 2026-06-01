// EdgeLLM — Example 03: Structured generation
//
// EdgeLLM's LLM client returns STRUCTURED OUTPUT only: you give a system + user
// message and a ResponseSchema, and the model returns a JSON object validated
// against it. This example asks Claude to describe a city as structured data.
// Target: ESP32.
//
// Prereqs:
//   1) Install ArduinoJson via the Library Manager.
//   2) Run `python3 tools/gen_ca_bundle.py` once to enable verified TLS.
//   3) Copy arduino_secrets.h.example -> arduino_secrets.h and fill it in.
//
// Swap AnthropicProvider for OpenAIProvider / GeminiProvider / OllamaProvider to
// use a different backend — the rest of the sketch is identical.

#include <EdgeLLM.h>

#include "arduino_secrets.h"

edge::Logger logger;
edge::SerialLogSink serialSink;
edge::NtpTimeSource ntp;

edge::AnthropicProvider provider(ANTHROPIC_API_KEY);
edge::ArduinoSecureConnection conn;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  logger.setSink(&serialSink);
  logger.setLevel(edge::LogLevel::Info);
  logger.registerSecret(WIFI_PASSWORD);
  logger.registerSecret(ANTHROPIC_API_KEY);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(250);
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
  client.options().maxTokens = 300;

  // Define the shape of the answer we want back.
  edge::ResponseSchema schema("city_facts");
  schema.field("name", edge::ParamType::String, "the city name")
      .field("country", edge::ParamType::String, "the country")
      .field("population", edge::ParamType::Integer, "approximate population")
      .field("famous_for", edge::ParamType::String, "one thing it's known for");

  logger.info("asking Claude for structured city facts...");
  edge::Result<edge::StructuredResult> r =
      client.generate(schema, "You return concise, accurate facts.", "Tell me about Kyoto, Japan.");
  if (!r.isOk()) {
    logger.error(std::string("generate failed: ") + r.message());
    return;
  }

  const edge::StructuredResult& out = r.value();
  logger.info(std::string("name:       ") + out.getString("name"));
  logger.info(std::string("country:    ") + out.getString("country"));
  logger.info(std::string("population: ") + String((long)out.getInt("population")).c_str());
  logger.info(std::string("famous for: ") + out.getString("famous_for"));
  logger.info(std::string("raw json:   ") + out.json());
}

void loop() { delay(1000); }
