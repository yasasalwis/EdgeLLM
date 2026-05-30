// EdgeLLM — SSRF guardrails for tools that fetch user-supplied URLs.
// Arduino-independent and pure. The agent loop itself never fetches URLs, but a
// tool handler might (e.g. "fetch this page"). If the URL is attacker-influenced
// (the model was prompted by untrusted input), it could be steered at internal
// hosts or a cloud metadata endpoint. Handlers should call isUrlAllowed() before
// connecting, and refuse when it returns false.
#ifndef EDGELLM_TOOLS_URLGUARD_H
#define EDGELLM_TOOLS_URLGUARD_H

#include <string>

namespace edge {

// Extracts the host portion (no scheme, no port, no path) from a URL. Returns
// an empty string if no host can be found.
std::string hostFromUrl(const std::string& url);

// True if `host` (a hostname or IP literal) resolves to a loopback, private,
// link-local, unique-local, or cloud-metadata address, or an obviously local
// name. Such hosts are unsafe destinations for a fetch driven by untrusted input.
bool isBlockedHost(const std::string& host);

// Convenience: true if the URL is safe to fetch (https/http scheme and a
// non-blocked host). Conservative — unknown shapes are rejected.
bool isUrlAllowed(const std::string& url);

}  // namespace edge

#endif  // EDGELLM_TOOLS_URLGUARD_H
