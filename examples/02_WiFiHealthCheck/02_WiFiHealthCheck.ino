// EdgeLLM — Example 02: WiFi + HTTP(S) Health Check
//
// Proves the end-to-end transport on real hardware: connect to WiFi, sync time
// over NTP, and perform an HTTP request through EdgeLLM's HttpClient, parsing
// the real response.
//
// Primary target: ESP32 (the Phase 1 reference board). The plain-HTTP path also
// works on other WiFi cores. The HTTPS path additionally requires a root CA
// bundle: run `python3 tools/gen_ca_bundle.py` once to populate it, then the
// secure check below activates automatically.
//
// Setup: copy arduino_secrets.h.example to arduino_secrets.h and fill in WiFi.

#include <EdgeLLM.h>

#include "arduino_secrets.h"

edge::Logger logger;
edge::SerialLogSink serialSink;
edge::NtpTimeSource clock_;

namespace {

void connectWiFi() {
  logger.info(std::string("connecting to WiFi: ") + WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 20000) {
      logger.error("WiFi connection timed out");
      return;
    }
    delay(250);
  }
  logger.info("WiFi connected");
}

// Performs one GET through EdgeLLM over the supplied connection and logs the
// outcome. Returns true on a 2xx/204 response.
bool healthCheck(edge::IConnection& conn, const char* host, uint16_t port, const char* path) {
  edge::HttpClient http(conn, &logger);
  http.setClock(edge::edgeArduinoMillis);
  http.setTimeout(10000);

  edge::HttpRequest req;
  req.method = "GET";
  req.host = host;
  req.port = port;
  req.path = path;
  req.setHeader("User-Agent", "EdgeLLM/" EDGELLM_VERSION);

  edge::HttpResponse resp;
  edge::Status s = http.send(req, resp);
  conn.stop();

  if (!s.isOk()) {
    logger.error(std::string("request failed: ") + s.message());
    return false;
  }
  logger.info(std::string("HTTP ") + String(resp.status).c_str() + " " + resp.reason + " (body " +
              String(static_cast<unsigned long>(resp.body.size())).c_str() + " bytes)");
  return resp.status == 204 || resp.isSuccess();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  logger.setSink(&serialSink);
  logger.setLevel(edge::LogLevel::Debug);
  logger.registerSecret(WIFI_PASSWORD);  // never let it leak into logs

  logger.info(std::string("EdgeLLM v") + EDGELLM_VERSION + " health check on " +
              edge::detectCapabilities().board);

  connectWiFi();
  if (WiFi.status() != WL_CONNECTED) return;

  // Time sync is required for TLS certificate validity checks.
  if (clock_.begin()) {
    logger.info(std::string("time synced, epoch=") +
                String(static_cast<unsigned long>(clock_.epoch())).c_str());
  } else {
    logger.warn("NTP sync failed; HTTPS certificate validation may fail");
  }

  // 1) Plain-HTTP health check — always available, proves WiFi + transport.
  {
    logger.info("plain-HTTP health check -> connectivitycheck.gstatic.com");
    edge::ArduinoPlainConnection conn;
    bool ok = healthCheck(conn, "connectivitycheck.gstatic.com", 80, "/generate_204");
    logger.info(std::string("plain-HTTP result: ") + (ok ? "PASS" : "FAIL"));
  }

  // 2) HTTPS health check — only if a CA bundle was generated. This exercises
  //    verifying TLS, the path used for cloud LLM APIs in Phase 2.
#if EDGELLM_HAS_CA_BUNDLE
  {
    logger.info("HTTPS health check -> www.gstatic.com (verifying TLS)");
    edge::ArduinoSecureConnection conn;
    conn.trust().useDefaultBundle();
    bool ok = healthCheck(conn, "www.gstatic.com", 443, "/generate_204");
    logger.info(std::string("HTTPS result: ") + (ok ? "PASS" : "FAIL"));
  }
#else
  logger.warn(
      "HTTPS check skipped: no CA bundle. Run `python3 tools/gen_ca_bundle.py` "
      "to enable verified TLS, then re-flash.");
#endif

  logger.info("health check complete");
}

void loop() { delay(1000); }
