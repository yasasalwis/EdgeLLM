// EdgeLLM — HTTP request/response value types.
// Arduino-independent. Header names are compared case-insensitively, matching
// RFC 7230.
#ifndef EDGELLM_TRANSPORT_HTTPTYPES_H
#define EDGELLM_TRANSPORT_HTTPTYPES_H

#include <string>
#include <utility>
#include <vector>

namespace edge {

using Header = std::pair<std::string, std::string>;

// ASCII-only case-insensitive equality, used for header-name matching. Defined
// here so both request and response types share one implementation.
bool headerNameEquals(const std::string& a, const std::string& b);

struct HttpRequest {
  std::string method = "GET";
  std::string host;
  uint16_t port = 443;
  std::string path = "/";
  std::vector<Header> headers;
  std::string body;

  // Sets (replacing any existing) a header. Name match is case-insensitive.
  void setHeader(const std::string& name, const std::string& value);

  // Appends a header without checking for duplicates (for multi-value headers).
  void addHeader(const std::string& name, const std::string& value) {
    headers.emplace_back(name, value);
  }

  // Returns the value of the first header matching `name`, or nullptr.
  const std::string* header(const std::string& name) const;
};

struct HttpResponse {
  int status = 0;            // e.g. 200
  std::string reason;        // e.g. "OK"
  std::vector<Header> headers;
  std::string body;

  bool isSuccess() const { return status >= 200 && status < 300; }

  // Returns the value of the first header matching `name`, or nullptr.
  const std::string* header(const std::string& name) const;
};

}  // namespace edge

#endif  // EDGELLM_TRANSPORT_HTTPTYPES_H
