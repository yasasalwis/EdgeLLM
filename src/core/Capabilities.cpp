#include "Capabilities.h"

#include "../hal/Platform.h"

namespace edge {

Capabilities detectCapabilities() {
  Capabilities c;

#if defined(EDGELLM_PLATFORM_ESP32)
  c.board = "esp32";
  c.heapBudget = 160000;  // typical free heap after WiFi+TLS on a classic ESP32
  c.maxTlsConnections = 4;
  c.supportsFullDuplex = true;       // FreeRTOS
  c.supportsPersistentStore = true;  // NVS / Preferences
  c.recvBufferSize = 1024;
  c.maxResponseBody = 65536;
#if defined(BOARD_HAS_PSRAM)
  c.hasPsram = true;
  c.maxResponseBody = 262144;
#endif

#elif defined(EDGELLM_PLATFORM_ESP8266)
  c.board = "esp8266";
  c.heapBudget = 24000;  // tight: one TLS connection consumes most of the heap
  c.maxTlsConnections = 1;
  c.supportsFullDuplex = false;      // single core, cooperative only
  c.supportsPersistentStore = true;  // flash KV via Preferences/EEPROM
  c.recvBufferSize = 512;
  c.maxResponseBody = 8192;

#elif defined(EDGELLM_PLATFORM_UNO_R4)
  c.board = "uno_r4";
  c.heapBudget = 16000;  // RA4M1 32KB SRAM; WiFi via ESP32-S3 co-processor
  c.maxTlsConnections = 1;
  c.supportsFullDuplex = false;
  c.supportsPersistentStore = true;  // on-chip flash data area
  c.recvBufferSize = 512;
  c.maxResponseBody = 8192;

#elif defined(EDGELLM_PLATFORM_SAMD)
  c.board = "samd_nina";
  c.heapBudget = 24000;  // SAMD21/51 with WiFiNINA
  c.maxTlsConnections = 1;
  c.supportsFullDuplex = false;
  c.supportsPersistentStore = false;  // no standard NVS; sketch must provide
  c.recvBufferSize = 512;
  c.maxResponseBody = 8192;

#elif defined(EDGELLM_PLATFORM_MBED)
  c.board = "mbed_portenta";
  c.heapBudget = 200000;  // Portenta H7 is comparatively roomy
  c.maxTlsConnections = 4;
  c.supportsFullDuplex = true;  // mbed RTOS threads
  c.supportsPersistentStore = true;
  c.recvBufferSize = 1024;
  c.maxResponseBody = 131072;

#elif defined(EDGELLM_PLATFORM_NATIVE)
  c.board = "native";
  c.heapBudget = 8000000;
  c.hasPsram = true;
  c.maxTlsConnections = 16;
  c.supportsFullDuplex = true;
  c.supportsPersistentStore = false;  // host tests use the in-memory store
  c.recvBufferSize = 4096;
  c.maxResponseBody = 1048576;

#else
  // Unknown board: assume the most constrained profile so we never over-commit
  // memory we don't have.
  c.board = "unknown";
  c.heapBudget = 16000;
  c.maxTlsConnections = 1;
  c.supportsFullDuplex = false;
  c.supportsPersistentStore = false;
  c.recvBufferSize = 512;
  c.maxResponseBody = 8192;
#endif

  return c;
}

}  // namespace edge
