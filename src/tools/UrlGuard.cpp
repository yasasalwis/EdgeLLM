#include "UrlGuard.h"

#include <cstdlib>

namespace edge {

namespace {
std::string toLower(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

// Parses exactly four dotted decimal octets. Returns true if `host` is exactly
// four numeric octets (0-255) and fills them.
bool parseIpv4(const std::string& host, int octets[4]) {
  size_t start = 0;
  for (int i = 0; i < 4; ++i) {
    std::string part;
    if (i < 3) {
      const size_t dot = host.find('.', start);
      if (dot == std::string::npos) return false;
      part = host.substr(start, dot - start);
      start = dot + 1;
    } else {
      part = host.substr(start);
      if (part.find('.') != std::string::npos) return false;  // no 5th octet
    }
    if (part.empty() || part.size() > 3) return false;
    for (char c : part) {
      if (c < '0' || c > '9') return false;
    }
    const int value = std::atoi(part.c_str());
    if (value < 0 || value > 255) return false;
    octets[i] = value;
  }
  return true;
}
}  // namespace

std::string hostFromUrl(const std::string& url) {
  size_t start = 0;
  const size_t scheme = url.find("://");
  if (scheme != std::string::npos) start = scheme + 3;

  // The authority ends at the first '/', '?', or '#'.
  size_t end = url.size();
  for (size_t i = start; i < url.size(); ++i) {
    const char c = url[i];
    if (c == '/' || c == '?' || c == '#') {
      end = i;
      break;
    }
  }
  std::string authority = url.substr(start, end - start);

  // Strip optional userinfo (user[:pass]@host) — must happen before port split,
  // since the userinfo itself may contain a ':'.
  const size_t at = authority.find('@');
  if (at != std::string::npos) authority = authority.substr(at + 1);

  // Bracketed IPv6 literal: keep the [..] and ignore any trailing :port.
  if (!authority.empty() && authority[0] == '[') {
    const size_t close = authority.find(']');
    if (close != std::string::npos) return authority.substr(0, close + 1);
    return authority;
  }

  // Strip :port for hostnames / IPv4.
  const size_t colon = authority.find(':');
  if (colon != std::string::npos) authority = authority.substr(0, colon);
  return authority;
}

bool isBlockedHost(const std::string& hostIn) {
  if (hostIn.empty()) return true;
  const std::string host = toLower(hostIn);

  // Obvious local names.
  if (host == "localhost" || host == "metadata.google.internal") return true;
  if (host.size() >= 6 && host.compare(host.size() - 6, 6, ".local") == 0) return true;

  int o[4];
  if (parseIpv4(host, o)) {
    if (o[0] == 127) return true;                                 // loopback
    if (o[0] == 10) return true;                                  // private
    if (o[0] == 172 && o[1] >= 16 && o[1] <= 31) return true;     // private
    if (o[0] == 192 && o[1] == 168) return true;                  // private
    if (o[0] == 169 && o[1] == 254) return true;                  // link-local + metadata
    if (o[0] == 0) return true;                                   // "this" network
    if (o[0] == 100 && o[1] >= 64 && o[1] <= 127) return true;    // CGNAT
    return false;
  }

  // Basic IPv6 literal checks.
  if (host.find(':') != std::string::npos) {
    if (host == "::1" || host == "[::1]") return true;  // loopback
    const std::string h = (!host.empty() && host[0] == '[') ? host.substr(1) : host;
    if (h.compare(0, 4, "fe80") == 0) return true;  // link-local
    if (!h.empty() && (h[0] == 'f') && (h.size() > 1) &&
        (h[1] == 'c' || h[1] == 'd'))
      return true;  // unique-local fc00::/7
    return false;
  }

  return false;
}

bool isUrlAllowed(const std::string& url) {
  const std::string lower = toLower(url);
  const bool http = lower.compare(0, 7, "http://") == 0;
  const bool https = lower.compare(0, 8, "https://") == 0;
  if (!http && !https) return false;  // only http(s)
  return !isBlockedHost(hostFromUrl(url));
}

}  // namespace edge
