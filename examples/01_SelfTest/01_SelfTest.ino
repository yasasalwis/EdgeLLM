// EdgeLLM — Example 01: Self Test (no network required)
//
// Runs on ANY supported board. Proves the library compiles, links and boots,
// and exercises the foundation pieces: version/capability detection, the
// redacting logger, and the in-memory secret store. Open the Serial Monitor at
// 115200 baud.
//
// This is the smoke test referenced by Phase 1: if this runs and prints a
// capability report, your toolchain and the library are wired up correctly.

#include <EdgeLLM.h>

edge::Logger logger;
edge::SerialLogSink serialSink;
edge::MemorySecretStore secrets;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    // Wait briefly for native-USB boards (Uno R4, Nano 33 IoT) to enumerate.
  }

  logger.setSink(&serialSink);
  logger.setLevel(edge::LogLevel::Debug);

  logger.info(std::string("EdgeLLM v") + EDGELLM_VERSION + " self test");

  // 1) Capability report for this board.
  edge::Capabilities caps = edge::detectCapabilities();
  logger.info(std::string("board: ") + caps.board);
  logger.info(std::string("heap budget (bytes): ") +
              String(static_cast<unsigned long>(caps.heapBudget)).c_str());
  logger.info(std::string("max TLS connections: ") +
              String(static_cast<int>(caps.maxTlsConnections)).c_str());
  logger.info(std::string("full duplex (FreeRTOS): ") + (caps.supportsFullDuplex ? "yes" : "no"));
  logger.info(std::string("persistent secret store: ") +
              (caps.supportsPersistentStore ? "yes" : "no"));

  // 2) Secret store round-trip (RAM only in this example).
  secrets.set("demo_key", "sk-demo-abcdef-123456");
  edge::Result<std::string> got = secrets.get("demo_key");
  logger.info(std::string("stored & read back demo_key: ") + (got.isOk() ? "ok" : "FAILED"));

  // 3) Redaction: register the secret, then deliberately log it. The logger
  //    must mask it. This demonstrates the defense-in-depth log scrubbing.
  if (got.isOk()) {
    logger.registerSecret(got.value());
    logger.info(std::string("masked form for logs: ") + edge::Logger::maskToken(got.value()));
    logger.warn(std::string("this line mentions the key ") + got.value() +
                " and must appear redacted");
  }

  logger.info("self test complete");
}

void loop() {
  // Nothing to do; the self test runs once in setup().
  delay(1000);
}
