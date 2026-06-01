#include "HttpTypes.h"

namespace edge {

namespace {
char asciiLower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
}  // namespace

bool headerNameEquals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (asciiLower(a[i]) != asciiLower(b[i])) return false;
  }
  return true;
}

void HttpRequest::setHeader(const std::string& name, const std::string& value) {
  for (auto& h : headers) {
    if (headerNameEquals(h.first, name)) {
      h.second = value;
      return;
    }
  }
  headers.emplace_back(name, value);
}

const std::string* HttpRequest::header(const std::string& name) const {
  for (const auto& h : headers) {
    if (headerNameEquals(h.first, name)) return &h.second;
  }
  return nullptr;
}

const std::string* HttpResponse::header(const std::string& name) const {
  for (const auto& h : headers) {
    if (headerNameEquals(h.first, name)) return &h.second;
  }
  return nullptr;
}

}  // namespace edge
