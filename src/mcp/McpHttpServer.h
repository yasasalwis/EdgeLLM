// EdgeLLM — MCP Streamable-HTTP transport (device glue).
// Guarded to Arduino targets. A minimal HTTP/1.1 server (built on the portable
// WiFiServer available on every supported core) that accepts MCP POSTs on a
// single endpoint, hands the body to McpServer, and returns the JSON response.
//
// Scope: implements the request/response leg of Streamable HTTP (POST -> JSON
// response, with Mcp-Session-Id). It returns 405 for GET, which the spec
// explicitly permits when a server does not offer a server-to-client SSE stream;
// server-initiated streaming is a later enhancement.
#ifndef EDGELLM_MCP_MCPHTTPSERVER_H
#define EDGELLM_MCP_MCPHTTPSERVER_H

#include <cstdint>

#include "../hal/Platform.h"

#if defined(EDGELLM_HAS_ARDUINO)

#if defined(EDGELLM_PLATFORM_ESP32)
#include <WiFi.h>
#elif defined(EDGELLM_PLATFORM_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(EDGELLM_PLATFORM_RP2040)
#include <WiFi.h>
#elif defined(EDGELLM_PLATFORM_UNO_R4)
#include <WiFiS3.h>
#elif defined(EDGELLM_PLATFORM_SAMD)
#include <WiFiNINA.h>
#elif defined(EDGELLM_PLATFORM_MBED)
#include <WiFi.h>
#endif

#include "McpServer.h"

namespace edge {

class McpHttpServer {
 public:
  McpHttpServer(McpServer& server, uint16_t port = 8080, const char* path = "/mcp")
      : mcp_(server), server_(port), port_(port), path_(path) {}

  // Starts listening. Call after WiFi is connected.
  void begin() { server_.begin(); }

  // Advertises this endpoint over mDNS/DNS-SD: the device becomes
  // <hostname>.local with a `_mcp._tcp` service carrying the port and a `path`
  // TXT record — so hosts on the LAN can find it without knowing the IP.
  // Supported on ESP32 and ESP8266; returns false on other boards (advertise
  // manually with your core's mDNS library) or if mDNS failed to start. Call
  // after WiFi is connected.
  bool advertise(const char* hostname);

  // Services at most one client connection. Call frequently from loop().
  void handle();

  // Caps to bound memory against malformed/oversized requests.
  void setMaxBodyBytes(uint32_t bytes) { maxBody_ = bytes; }
  void setReadTimeoutMs(uint32_t ms) { readTimeoutMs_ = ms; }

  // Delay applied before answering a 401, to throttle bearer-token brute-force
  // over the LAN. Set to 0 to disable.
  void setAuthFailDelayMs(uint32_t ms) { authFailDelayMs_ = ms; }

 private:
  void sendResponse(WiFiClient& client, int status, const std::string& body,
                    const std::string& sessionId, bool withSession);

  McpServer& mcp_;
  WiFiServer server_;
  uint16_t port_;
  std::string path_;
  uint32_t maxBody_ = 16384;
  uint32_t readTimeoutMs_ = 5000;
  uint32_t authFailDelayMs_ = 500;
  bool mdnsActive_ = false;
};

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
#endif  // EDGELLM_MCP_MCPHTTPSERVER_H
