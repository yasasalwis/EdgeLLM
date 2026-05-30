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

#endif  // EDGELLM_HAL_PLATFORM_H
