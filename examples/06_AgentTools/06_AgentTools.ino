// EdgeLLM — Example 06: Agent with on-device tools
//
// Registers two device tools and lets Claude call them to satisfy a request:
//   * get_uptime  (read)  — returns how long the board has been running
//   * set_led     (write) — turns the built-in LED on/off  [mutating]
//
// The LLMClient agent loop runs the requested tools on-device, feeds the results
// back, and loops until Claude gives a final answer. Target: ESP32.
//
// Prereqs: ArduinoJson installed, `tools/gen_ca_bundle.py` run, secrets filled.

#include <EdgeLLM.h>

#include "arduino_secrets.h"

edge::Logger logger;
edge::SerialLogSink serialSink;
edge::NtpTimeSource ntp;

edge::AnthropicProvider provider(ANTHROPIC_API_KEY);
edge::ArduinoSecureConnection conn;
edge::ToolRegistry tools;

namespace {
void registerTools() {
  tools.addTool("get_uptime", "Get how long the device has been running, in seconds")
      .onCall([](edge::ToolCallArgs&) {
        return edge::ToolResult::ok(String(millis() / 1000).c_str());
      });

  tools.addTool("set_led", "Turn the built-in LED on or off")
      .paramEnum("state", "on or off", {"on", "off"}, true)
      .mutating()       // annotated as a write
      .allowWrite()     // explicitly permitted (matters for the MCP server later)
      .onCall([](edge::ToolCallArgs& args) {
        const std::string state = args.getString("state");
        digitalWrite(LED_BUILTIN, state == "on" ? HIGH : LOW);
        return edge::ToolResult::ok("LED is now " + state);
      });
}
}  // namespace

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
  }
  logger.setSink(&serialSink);
  logger.setLevel(edge::LogLevel::Info);
  logger.registerSecret(WIFI_PASSWORD);
  logger.registerSecret(ANTHROPIC_API_KEY);

  pinMode(LED_BUILTIN, OUTPUT);
  registerTools();

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
  client.options().system =
      "You control an ESP32. Use the available tools to fulfill requests, then "
      "summarize what you did in one sentence.";
  client.options().maxTokens = 300;

  logger.info("asking the agent to turn on the LED and report uptime...");
  edge::Result<edge::ChatResult> r =
      client.run("Turn the LED on, then tell me how long the device has been running.", tools);
  if (!r.isOk()) {
    logger.error(std::string("agent failed: ") + r.message());
    return;
  }
  logger.info(std::string("agent: ") + r.value().text);
}

void loop() { delay(1000); }
