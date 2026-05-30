#include "McpHttpServer.h"

#if defined(EDGELLM_HAS_ARDUINO)

#include <Arduino.h>

namespace edge {

namespace {
constexpr size_t kMaxLineLen = 2048;

const char* statusText(int status) {
  switch (status) {
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    default: return "Error";
  }
}

char asciiLower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

bool headerIs(const std::string& name, const char* want) {
  size_t n = 0;
  for (; want[n]; ++n) {
    if (n >= name.size() || asciiLower(name[n]) != want[n]) return false;
  }
  return n == name.size();
}

bool readLine(WiFiClient& client, std::string& out, uint32_t start, uint32_t timeoutMs) {
  out.clear();
  for (;;) {
    if (millis() - start > timeoutMs) return false;
    int c = client.read();
    if (c < 0) {
      if (!client.connected() && client.available() == 0) return false;
      delay(1);
      continue;
    }
    if (c == '\n') {
      if (!out.empty() && out.back() == '\r') out.pop_back();
      return true;
    }
    out.push_back(static_cast<char>(c));
    if (out.size() > kMaxLineLen) return false;
  }
}
}  // namespace

void McpHttpServer::sendResponse(WiFiClient& client, int status, const std::string& body,
                                 const std::string& sessionId, bool withSession) {
  client.print("HTTP/1.1 ");
  client.print(status);
  client.print(" ");
  client.println(statusText(status));
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(static_cast<unsigned long>(body.size()));
  if (withSession) {
    client.print("Mcp-Session-Id: ");
    client.println(sessionId.c_str());
  }
  client.println("Connection: close");
  client.println();
  if (!body.empty()) client.print(body.c_str());
}

void McpHttpServer::handle() {
  WiFiClient client = server_.available();
  if (!client) return;

  const uint32_t start = millis();
  std::string method;
  std::string path;
  std::string authHeader;
  long contentLength = 0;

  // Request line.
  std::string line;
  if (!readLine(client, line, start, readTimeoutMs_)) {
    client.stop();
    return;
  }
  {
    const size_t sp1 = line.find(' ');
    const size_t sp2 = (sp1 == std::string::npos) ? std::string::npos : line.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) {
      sendResponse(client, 400, "{\"error\":\"bad request line\"}", "", false);
      client.stop();
      return;
    }
    method = line.substr(0, sp1);
    path = line.substr(sp1 + 1, sp2 - sp1 - 1);
    const size_t q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);  // strip query
  }

  // Headers.
  for (;;) {
    if (!readLine(client, line, start, readTimeoutMs_)) {
      client.stop();
      return;
    }
    if (line.empty()) break;  // end of headers
    const size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string name = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
    if (headerIs(name, "content-length")) {
      contentLength = atol(value.c_str());
    } else if (headerIs(name, "authorization")) {
      authHeader = value;
    }
  }

  if (contentLength < 0 || static_cast<uint32_t>(contentLength) > maxBody_) {
    sendResponse(client, 413, "{\"error\":\"body too large\"}", "", false);
    client.stop();
    return;
  }

  // Body.
  std::string body;
  body.reserve(static_cast<size_t>(contentLength));
  const uint32_t bodyStart = millis();
  while (static_cast<long>(body.size()) < contentLength) {
    if (millis() - bodyStart > readTimeoutMs_) break;
    int c = client.read();
    if (c < 0) {
      if (!client.connected() && client.available() == 0) break;
      delay(1);
      continue;
    }
    body.push_back(static_cast<char>(c));
  }

  const bool pathMatch = (path == path_);
  if (method == "POST" && pathMatch) {
    const bool authorized = mcp_.checkAuth(authHeader);
    McpReply reply = mcp_.handlePost(body, authorized);
    // Throttle bearer-token brute-force: pause before answering a rejection.
    if (reply.httpStatus == 401 && authFailDelayMs_ > 0) delay(authFailDelayMs_);
    sendResponse(client, reply.httpStatus, reply.body, reply.sessionId, reply.setSession);
  } else if (method == "GET" && pathMatch) {
    // No server-initiated SSE stream; the spec permits 405 here.
    sendResponse(client, 405, "{\"error\":\"SSE stream not supported\"}", "", false);
  } else {
    sendResponse(client, 404, "{\"error\":\"not found\"}", "", false);
  }
  client.stop();
}

}  // namespace edge

#endif  // EDGELLM_HAS_ARDUINO
