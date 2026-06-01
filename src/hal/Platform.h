// EdgeLLM — platform detection.
// Translates the many core-specific predefined macros into one stable set of
// EDGELLM_PLATFORM_* flags used throughout the library. Include this instead of
// testing core macros directly.
#ifndef EDGELLM_HAL_PLATFORM_H
#define EDGELLM_HAL_PLATFORM_H

#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
#define EDGELLM_PLATFORM_ESP32 1
#define EDGELLM_HAS_ARDUINO 1

#elif defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266)
#define EDGELLM_PLATFORM_ESP8266 1
#define EDGELLM_HAS_ARDUINO 1

#elif defined(ARDUINO_UNOR4_WIFI) || defined(ARDUINO_ARCH_RENESAS_UNO) || \
    defined(ARDUINO_ARCH_RENESAS)
#define EDGELLM_PLATFORM_UNO_R4 1
#define EDGELLM_HAS_ARDUINO 1

#elif defined(ARDUINO_ARCH_SAMD)
#define EDGELLM_PLATFORM_SAMD 1
#define EDGELLM_HAS_ARDUINO 1

#elif defined(ARDUINO_ARCH_MBED) || defined(ARDUINO_ARCH_MBED_PORTENTA) || \
    defined(ARDUINO_ARCH_MBED_NANO)
#define EDGELLM_PLATFORM_MBED 1
#define EDGELLM_HAS_ARDUINO 1

#elif defined(ARDUINO)
// Some other Arduino core we don't specifically tier. Treat as constrained.
#define EDGELLM_PLATFORM_GENERIC_ARDUINO 1
#define EDGELLM_HAS_ARDUINO 1

#else
// Host build (native unit tests). No Arduino runtime.
#define EDGELLM_PLATFORM_NATIVE 1
#endif

// ---------------------------------------------------------------------------
// Compile-time capability gate.
//
// EdgeLLM is built on the C++ STL (std::string, std::vector, std::function,
// std::map). Classic AVR Arduino (Uno/Mega + a WiFi shield) ships no libstdc++
// and only a few KB of RAM, so it cannot build or run this library. Rather than
// emit a wall of "std::string does not name a type" errors, fail early with a
// clear message pointing at the supported boards.
// ---------------------------------------------------------------------------
// clang-format off
#if defined(EDGELLM_HAS_ARDUINO)
#if defined(__has_include)
#if !__has_include(<string>)
#error "EdgeLLM is not supported on this board: it requires the C++ STL. Use a 32-bit WiFi core — ESP32, ESP8266, Arduino Uno R4 WiFi, Nano 33 IoT / MKR (WiFiNINA), or Portenta. Classic AVR Arduino (Uno/Mega) + WiFi shield is not supported."
#endif
#elif defined(__AVR__) || defined(ARDUINO_ARCH_AVR)
#error "EdgeLLM is not supported on AVR (classic Arduino Uno/Mega). Use a 32-bit WiFi core — ESP32, ESP8266, Uno R4 WiFi, Nano 33 IoT / MKR, or Portenta."
#endif
#endif
// clang-format on

#endif  // EDGELLM_HAL_PLATFORM_H
