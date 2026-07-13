// EdgeLLM — on-device IConnection backed by an Arduino networking Client.
// Only compiled for Arduino targets (guarded); native tests use FakeConnection.
//
// ArduinoSecureConnection wraps the platform's TLS client (WiFiClientSecure on
// ESP32/ESP8266, WiFiSSLClient on Uno R4 / WiFiNINA boards) and applies the
// CACertStore trust policy. ArduinoPlainConnection is unencrypted TCP, intended
// for a trusted local endpoint such as Ollama.
#ifndef EDGELLM_HAL_ARDUINOCONNECTION_H
#define EDGELLM_HAL_ARDUINOCONNECTION_H

#include <cstdint>

#include "Platform.h"

#if defined(EDGELLM_HAS_ARDUINO)

#include "../transport/CACertStore.h"
#include "../transport/IConnection.h"

#if defined(EDGELLM_PLATFORM_ESP32)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#elif defined(EDGELLM_PLATFORM_ESP8266)
#include <ESP8266WiFi.h>
#elif defined(EDGELLM_PLATFORM_RP2040)
#include <WiFi.h>  // arduino-pico: ESP8266-compatible BearSSL stack
#elif defined(EDGELLM_PLATFORM_UNO_R4)
#include <WiFiS3.h>
#elif defined(EDGELLM_PLATFORM_SAMD)
#include <WiFiNINA.h>
#elif defined(EDGELLM_PLATFORM_MBED)
#include <WiFi.h>
#endif

namespace edge {

// Selects the concrete TLS client type for the current core.
#if defined(EDGELLM_PLATFORM_ESP32)
using PlatformSecureClient = WiFiClientSecure;
using PlatformPlainClient = WiFiClient;
#elif defined(EDGELLM_PLATFORM_ESP8266) || defined(EDGELLM_PLATFORM_RP2040)
using PlatformSecureClient = BearSSL::WiFiClientSecure;
using PlatformPlainClient = WiFiClient;
#else
using PlatformSecureClient = WiFiSSLClient;
using PlatformPlainClient = WiFiClient;
#endif

class ArduinoSecureConnection : public IConnection {
 public:
  ArduinoSecureConnection() = default;

  // Trust policy applied before each connect. Configure via
  // trust().setCACert(...) / trust().setInsecure().
  CACertStore& trust() { return trust_; }

  Status connect(const char* host, uint16_t port) override;
  bool connected() override { return client_.connected(); }
  int write(const uint8_t* data, size_t len) override;
  int read(uint8_t* buf, size_t len) override;
  int available() override { return client_.available(); }
  void stop() override { client_.stop(); }

 private:
  void applyTrust();

  PlatformSecureClient client_;
  CACertStore trust_;
#if defined(EDGELLM_PLATFORM_ESP8266) || defined(EDGELLM_PLATFORM_RP2040)
  BearSSL::X509List* anchors_ = nullptr;
#endif
};

class ArduinoPlainConnection : public IConnection {
 public:
  Status connect(const char* host, uint16_t port) override;
  bool connected() override { return client_.connected(); }
  int write(const uint8_t* data, size_t len) override;
  int read(uint8_t* buf, size_t len) override;
  int available() override { return client_.available(); }
  void stop() override { client_.stop(); }

 private:
  PlatformPlainClient client_;
};

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
#endif  // EDGELLM_HAL_ARDUINOCONNECTION_H
