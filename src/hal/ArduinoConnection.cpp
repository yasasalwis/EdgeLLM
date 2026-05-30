#include "ArduinoConnection.h"

#if defined(EDGELLM_HAS_ARDUINO)

namespace edge {

void ArduinoSecureConnection::applyTrust() {
#if defined(EDGELLM_PLATFORM_ESP32)
  if (trust_.insecure()) {
    client_.setInsecure();
  } else if (trust_.hasCert()) {
    client_.setCACert(trust_.pem());
  }
#elif defined(EDGELLM_PLATFORM_ESP8266)
  if (trust_.insecure()) {
    client_.setInsecure();
  } else if (trust_.hasCert()) {
    if (anchors_ != nullptr) delete anchors_;
    anchors_ = new BearSSL::X509List(trust_.pem());
    client_.setTrustAnchors(anchors_);
  }
#else
  // Uno R4 / WiFiNINA boards manage the trust store inside the WiFi co-processor
  // firmware; per-connection CA injection is added in a later phase. Insecure
  // mode is honored where the client exposes it.
  (void)trust_;
#endif
}

Status ArduinoSecureConnection::connect(const char* host, uint16_t port) {
  applyTrust();
  const int ok = client_.connect(host, port);
  if (ok != 1) {
    // Distinguish a verification failure from a transport failure where the
    // core exposes it (ESP32).
#if defined(EDGELLM_PLATFORM_ESP32)
    char err[128];
    const int code = client_.lastError(err, sizeof(err));
    if (code != 0 && !trust_.insecure()) return Status::fail(Error::CertVerifyFailed);
#endif
    return Status::fail(Error::ConnectFailed);
  }
  return Status::ok();
}

int ArduinoSecureConnection::write(const uint8_t* data, size_t len) {
  const size_t n = client_.write(data, len);
  return static_cast<int>(n);
}

int ArduinoSecureConnection::read(uint8_t* buf, size_t len) {
  if (client_.available() <= 0) {
    return client_.connected() ? 0 : -1;
  }
  const int n = client_.read(buf, len);
  return n;
}

Status ArduinoPlainConnection::connect(const char* host, uint16_t port) {
  const int ok = client_.connect(host, port);
  return ok == 1 ? Status::ok() : Status::fail(Error::ConnectFailed);
}

int ArduinoPlainConnection::write(const uint8_t* data, size_t len) {
  return static_cast<int>(client_.write(data, len));
}

int ArduinoPlainConnection::read(uint8_t* buf, size_t len) {
  if (client_.available() <= 0) {
    return client_.connected() ? 0 : -1;
  }
  return client_.read(buf, len);
}

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
