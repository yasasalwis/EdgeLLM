// EdgeLLM — Example 07: MCP Server (expose your device to LLM hosts)
//
// Turns the ESP32 into a Model Context Protocol server that any MCP host (Claude
// Desktop, MCP Inspector, your own client) can connect to over the LAN to:
//   * call tools:      get_temp (read), set_led (write, deny-by-default opt-in)
//   * read resources:  status://device (live device status JSON)
//   * read/write data: the built-in EdgeStore KV (kv://<key>, kv_set, kv_delete)
//
// After it boots, point an MCP client at  http://<device-ip>:8080/mcp
//
// Test quickly with the MCP Inspector:
//   npx @modelcontextprotocol/inspector
//   (Transport: Streamable HTTP, URL: http://<device-ip>:8080/mcp)

#include <EdgeLLM.h>

#include "arduino_secrets.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2  // most ESP32 dev boards expose the onboard LED on GPIO2
#endif

edge::Logger logger;
edge::SerialLogSink serialSink;

edge::ToolRegistry tools;
edge::ResourceRegistry resources;
edge::EdgeStore store;
edge::PreferencesSecretStore kvBackend("edgellm_kv");  // NVS persistence for the KV store

edge::McpServer mcp("EdgeLLM-Device", EDGELLM_VERSION);
edge::McpHttpServer http(mcp, 8080, "/mcp");

namespace {
void setupTools() {
  tools.addTool("get_temp", "Read the current temperature in Celsius")
      .onCall([](edge::ToolCallArgs&) {
        // Replace with a real sensor read; ESP32 has an internal temp sensor.
        return edge::ToolResult::ok("21.5");
      });

  tools.addTool("set_led", "Turn the built-in LED on or off")
      .paramEnum("state", "on or off", {"on", "off"}, true)
      .mutating()
      .allowWrite()  // explicit opt-in (writes are deny-by-default)
      .onCall([](edge::ToolCallArgs& a) {
        digitalWrite(LED_BUILTIN, a.getString("state") == "on" ? HIGH : LOW);
        return edge::ToolResult::ok("LED " + a.getString("state"));
      });

  resources.addResource("status://device", "Device status")
      .mimeType("application/json")
      .onRead([](const std::string&) {
        std::string json = "{\"uptime_s\":";
        json += String(millis() / 1000).c_str();
        json += ",\"free_heap\":";
        json += String((unsigned long)ESP.getFreeHeap()).c_str();
        json += "}";
        return edge::Result<std::string>::ok(json);
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

  pinMode(LED_BUILTIN, OUTPUT);
  setupTools();

  store.setPersistence(&kvBackend);  // KV survives reboots via NVS

  mcp.setInstructions("An ESP32 running EdgeLLM. Use tools to read sensors and control the LED.");
  mcp.setToolRegistry(&tools);
  mcp.setResourceRegistry(&resources);
  mcp.setStore(&store, /*allowWrites=*/true);  // expose kv_set / kv_delete
#if defined(MCP_BEARER_TOKEN)
  if (sizeof(MCP_BEARER_TOKEN) > 1) {  // non-empty token
    mcp.setAuthToken(MCP_BEARER_TOKEN);
    logger.registerSecret(MCP_BEARER_TOKEN);
    logger.info("MCP bearer auth enabled");
  }
#endif

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(250);
  if (WiFi.status() != WL_CONNECTED) {
    logger.error("WiFi failed");
    return;
  }

  http.begin();
  logger.info(std::string("MCP server ready at http://") + WiFi.localIP().toString().c_str() +
              ":8080/mcp");
}

void loop() {
  http.handle();  // services one MCP request per call; keep this in loop()
  delay(2);
}
