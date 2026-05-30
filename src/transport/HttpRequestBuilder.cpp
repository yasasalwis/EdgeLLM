#include "HttpRequestBuilder.h"

namespace edge {

namespace {
constexpr char kCrlf[] = "\r\n";

bool hasHeader(const HttpRequest& req, const std::string& name) {
  return req.header(name) != nullptr;
}

std::string toDecimal(size_t value) {
  if (value == 0) return "0";
  char buf[20];
  size_t i = sizeof(buf);
  while (value > 0 && i > 0) {
    buf[--i] = static_cast<char>('0' + (value % 10));
    value /= 10;
  }
  return std::string(buf + i, sizeof(buf) - i);
}
}  // namespace

std::string serializeRequest(const HttpRequest& req) {
  std::string out;
  // Rough reserve to avoid repeated reallocations on constrained heaps.
  out.reserve(128 + req.body.size());

  // Request line: METHOD SP path SP HTTP/1.1 CRLF
  out += req.method;
  out += ' ';
  out += req.path.empty() ? "/" : req.path;
  out += " HTTP/1.1";
  out += kCrlf;

  // Host is mandatory in HTTP/1.1. Add it first if the caller didn't.
  if (!hasHeader(req, "Host")) {
    out += "Host: ";
    out += req.host;
    out += kCrlf;
  }

  // Emit caller-provided headers verbatim, in order.
  for (const auto& h : req.headers) {
    out += h.first;
    out += ": ";
    out += h.second;
    out += kCrlf;
  }

  // Content-Length when there is a body and the caller hasn't framed it itself
  // (e.g. via Transfer-Encoding: chunked).
  const bool chunked = hasHeader(req, "Transfer-Encoding");
  if (!req.body.empty() && !hasHeader(req, "Content-Length") && !chunked) {
    out += "Content-Length: ";
    out += toDecimal(req.body.size());
    out += kCrlf;
  }

  // Default to closing the connection after the response: simplest correct
  // behaviour on memory-constrained boards. Callers wanting keep-alive set it.
  if (!hasHeader(req, "Connection")) {
    out += "Connection: close";
    out += kCrlf;
  }

  out += kCrlf;  // end of headers
  out += req.body;
  return out;
}

}  // namespace edge
