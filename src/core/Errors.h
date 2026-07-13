// EdgeLLM — library-wide error codes.
// Arduino-independent. No exceptions are used anywhere in EdgeLLM; failures are
// returned as values (see Result.h) so the library is safe on microcontrollers
// where exception support is often disabled or expensive.
#ifndef EDGELLM_CORE_ERRORS_H
#define EDGELLM_CORE_ERRORS_H

#include <cstdint>

namespace edge {

// Stable error taxonomy shared by every layer. New codes are appended; existing
// numeric values are never reused so logs remain comparable across versions.
enum class Error : uint8_t {
  Ok = 0,

  // Generic / programming
  InvalidArgument = 1,
  InvalidState = 2,
  NotImplemented = 3,
  OutOfMemory = 4,
  Timeout = 5,
  NotFound = 6,
  AlreadyExists = 7,
  Capacity = 8,  // a bounded buffer / collection limit was reached

  // Network / transport (10-29)
  NotConnected = 10,
  ConnectFailed = 11,
  DnsFailed = 12,
  TlsHandshakeFailed = 13,
  CertVerifyFailed = 14,
  WriteFailed = 15,
  ReadFailed = 16,
  ConnectionClosed = 17,

  // HTTP (30-49)
  HttpMalformed = 30,
  HttpStatusError = 31,  // non-2xx; inspect HttpResponse.status
  HttpBodyTooLarge = 32,
  HttpUnsupportedTransfer = 33,

  // Parsing (50-69)
  JsonParseError = 50,
  SseMalformed = 51,
  SchemaValidationFailed = 52,

  // Auth / security (70-89)
  Unauthorized = 70,
  Forbidden = 71,
  SecretNotFound = 72,
  SecretStoreError = 73,

  // Provider / LLM (90-109)
  ProviderError = 90,
  RateLimited = 91,
  ContextOverflow = 92,
  ToolIterationLimit = 93,
  BudgetExceeded = 94,  // a UsageMeter cap (requests/tokens) was reached

  // MCP (110-129)
  McpProtocolError = 110,
  McpMethodNotFound = 111,
  McpToolNotFound = 112,
  McpWriteNotAllowed = 113,
};

// Human-readable, allocation-free description. Returned strings are static and
// never contain secrets or user data — safe to log and to surface to clients.
inline const char* errorString(Error e) {
  switch (e) {
    case Error::Ok:
      return "ok";
    case Error::InvalidArgument:
      return "invalid argument";
    case Error::InvalidState:
      return "invalid state";
    case Error::NotImplemented:
      return "not implemented";
    case Error::OutOfMemory:
      return "out of memory";
    case Error::Timeout:
      return "timeout";
    case Error::NotFound:
      return "not found";
    case Error::AlreadyExists:
      return "already exists";
    case Error::Capacity:
      return "capacity exceeded";
    case Error::NotConnected:
      return "not connected";
    case Error::ConnectFailed:
      return "connect failed";
    case Error::DnsFailed:
      return "dns resolution failed";
    case Error::TlsHandshakeFailed:
      return "tls handshake failed";
    case Error::CertVerifyFailed:
      return "certificate verification failed";
    case Error::WriteFailed:
      return "socket write failed";
    case Error::ReadFailed:
      return "socket read failed";
    case Error::ConnectionClosed:
      return "connection closed";
    case Error::HttpMalformed:
      return "malformed http response";
    case Error::HttpStatusError:
      return "http status error";
    case Error::HttpBodyTooLarge:
      return "http body too large";
    case Error::HttpUnsupportedTransfer:
      return "unsupported transfer encoding";
    case Error::JsonParseError:
      return "json parse error";
    case Error::SseMalformed:
      return "malformed sse stream";
    case Error::SchemaValidationFailed:
      return "schema validation failed";
    case Error::Unauthorized:
      return "unauthorized";
    case Error::Forbidden:
      return "forbidden";
    case Error::SecretNotFound:
      return "secret not found";
    case Error::SecretStoreError:
      return "secret store error";
    case Error::ProviderError:
      return "provider error";
    case Error::RateLimited:
      return "rate limited";
    case Error::ContextOverflow:
      return "context window overflow";
    case Error::ToolIterationLimit:
      return "tool iteration limit reached";
    case Error::BudgetExceeded:
      return "usage budget exceeded";
    case Error::McpProtocolError:
      return "mcp protocol error";
    case Error::McpMethodNotFound:
      return "mcp method not found";
    case Error::McpToolNotFound:
      return "mcp tool not found";
    case Error::McpWriteNotAllowed:
      return "mcp write not allowed";
  }
  return "unknown error";
}

}  // namespace edge

#endif  // EDGELLM_CORE_ERRORS_H
