// EdgeLLM — abstract byte-stream connection.
// Arduino-independent. Decouples HTTP/TLS logic from the concrete socket so the
// transport can be exercised in native tests with a scripted fake, and backed
// on-device by an Arduino `Client` (WiFiClientSecure, WiFiSSLClient, ...).
#ifndef EDGELLM_TRANSPORT_ICONNECTION_H
#define EDGELLM_TRANSPORT_ICONNECTION_H

#include <cstddef>
#include <cstdint>

#include "../core/Result.h"

namespace edge {

class IConnection {
 public:
  virtual ~IConnection() = default;

  // Opens a connection to host:port (TLS handshake included for secure impls).
  virtual Status connect(const char* host, uint16_t port) = 0;

  // True while the underlying socket is usable.
  virtual bool connected() = 0;

  // Writes exactly `len` bytes. Returns the number written (== len on success),
  // or a negative value on error.
  virtual int write(const uint8_t* data, size_t len) = 0;

  // Reads up to `len` bytes. Returns the count placed in `buf` (0 if none are
  // available right now), or a negative value if the connection is closed/errored.
  virtual int read(uint8_t* buf, size_t len) = 0;

  // Bytes immediately readable without blocking (>= 0), or negative on error.
  virtual int available() = 0;

  // Closes the connection. Safe to call when already closed.
  virtual void stop() = 0;
};

}  // namespace edge

#endif  // EDGELLM_TRANSPORT_ICONNECTION_H
