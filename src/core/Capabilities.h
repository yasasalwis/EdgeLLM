// EdgeLLM — per-board capability tiering.
// Arduino-independent header. The library degrades features based on the
// detected board so the same sketch behaves sensibly from an ESP32 down to an
// ESP8266. Detection is compile-time (board macros); a few fields are also
// queried at runtime where the core exposes them (e.g. free heap).
#ifndef EDGELLM_CORE_CAPABILITIES_H
#define EDGELLM_CORE_CAPABILITIES_H

#include <cstddef>
#include <cstdint>

namespace edge {

struct Capabilities {
  // Short, stable board family identifier (e.g. "esp32", "esp8266", "uno_r4").
  const char* board = "unknown";

  // Conservative steady-state free-heap estimate in bytes. Used to size default
  // buffers and to decide whether full-duplex is safe. Not a hard limit.
  uint32_t heapBudget = 0;

  // True if a second RAM pool (PSRAM) is available for large buffers.
  bool hasPsram = false;

  // Maximum simultaneous TLS connections the board can realistically sustain.
  // ESP8266 is effectively 1; ESP32 can do several.
  uint8_t maxTlsConnections = 1;

  // True if the platform has preemptive tasks (FreeRTOS) so the MCP server and
  // an outbound LLM call can run concurrently. False -> cooperative loop only.
  bool supportsFullDuplex = false;

  // True if a persistent key/value flash store (NVS/Preferences-equivalent) is
  // available for secrets and EdgeStore persistence.
  bool supportsPersistentStore = false;

  // Default size of the transport receive buffer. Smaller on tight boards.
  uint16_t recvBufferSize = 512;

  // Default maximum HTTP response body the transport will accept (bytes). Guards
  // against memory exhaustion; callers can override per request.
  uint32_t maxResponseBody = 16384;
};

// Returns the capability profile for the board this was compiled for. Pure with
// respect to compile-time macros; safe to call repeatedly.
Capabilities detectCapabilities();

}  // namespace edge

#endif  // EDGELLM_CORE_CAPABILITIES_H
