// EdgeLLM — Example 04: Structured data extraction
//
// Extracts structured fields from a free-text sentence using structured output
// (the model is constrained to the schema and the result is validated locally).
// Target: ESP32. Uses OpenAI here; any provider works identically.
//
// Prereqs: ArduinoJson installed, `tools/gen_ca_bundle.py` run, secrets filled.

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

  // Extract a calendar event from a sentence.
  edge::ResponseSchema schema("calendar_event");
  schema.field("title", edge::ParamType::String, "the event title")
      .field("day", edge::ParamType::String, "the day of week")
      .field("hour_24", edge::ParamType::Integer, "start hour in 24h time")
      .field("is_recurring", edge::ParamType::Boolean, "true if it repeats");

  const char* sentence = "Set up the weekly team standup every Monday at 9am.";
  logger.info(std::string("extracting from: ") + sentence);

  edge::Result<edge::StructuredResult> r =
      client.generate(schema, "Extract structured calendar events from text.", sentence);
  if (!r.isOk()) {
    logger.error(std::string("extraction failed: ") + r.message());
    return;
  }
  const edge::StructuredResult& e = r.value();
  logger.info(std::string("title:     ") + e.getString("title"));
  logger.info(std::string("day:       ") + e.getString("day"));
  logger.info(std::string("hour_24:   ") + String((long)e.getInt("hour_24")).c_str());
  logger.info(std::string("recurring: ") + (e.getBool("is_recurring") ? "yes" : "no"));
}

void loop() { delay(1000); }
