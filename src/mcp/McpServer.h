// EdgeLLM — Model Context Protocol server core (Feature B).
// Arduino-independent: processes a JSON-RPC 2.0 message (the body of an MCP
// Streamable-HTTP POST) and returns the JSON response. Because it is pure
// string-in/string-out, the entire protocol surface is unit-tested on the host;
// only the HTTP glue (McpHttpServer) is device-specific.
//
// Shares the Phase 3 ToolRegistry, so a tool registered once is callable by the
// on-device agent loop AND by networked MCP hosts. Security: mutating tools are
// deny-by-default (excluded from tools/list and refused by tools/call unless
// allowWrite() was set); an optional bearer token gates the whole endpoint.
//
// Implemented methods: initialize, ping, notifications/*, tools/list, tools/call,
// resources/list, resources/templates/list, resources/read, prompts/list,
// prompts/get.
#ifndef EDGELLM_MCP_MCPSERVER_H
#define EDGELLM_MCP_MCPSERVER_H

#include <cstdint>

#include <string>

#include "../core/Logger.h"
#include "../tools/ToolRegistry.h"
#include "EdgeStore.h"
#include "PromptRegistry.h"
#include "ResourceRegistry.h"

namespace edge {

struct McpReply {
  std::string body;                 // JSON response body (empty for notification-only)
  int httpStatus = 200;             // HTTP status the glue should send
  bool isNotificationOnly = false;  // true -> 202 Accepted, no body
  std::string sessionId;            // set on initialize
  bool setSession = false;          // glue should emit Mcp-Session-Id
};

class McpServer {
 public:
  McpServer(std::string name, std::string version)
      : name_(std::move(name)), version_(std::move(version)) {}

  void setInstructions(std::string text) { instructions_ = std::move(text); }
  void setProtocolVersion(std::string v) { protocolVersion_ = std::move(v); }
  void setLogger(Logger* logger) { logger_ = logger; }

  // Requires "Authorization: Bearer <token>" on every request. Empty -> no auth.
  void setAuthToken(std::string token) { authToken_ = std::move(token); }
  bool authRequired() const { return !authToken_.empty(); }
  // Helper for the HTTP glue: validate an Authorization header value.
  bool checkAuth(const std::string& authorizationHeader) const;

  void setToolRegistry(ToolRegistry* tools) { tools_ = tools; }
  void setResourceRegistry(ResourceRegistry* resources) { resources_ = resources; }
  void setPromptRegistry(PromptRegistry* prompts) { prompts_ = prompts; }
  // Attaches the built-in KV store. `allowWrites` exposes kv_set/kv_delete tools
  // (deny-by-default: off unless explicitly enabled).
  void setStore(EdgeStore* store, bool allowWrites = false) {
    store_ = store;
    storeWrites_ = allowWrites;
  }

  // Processes one POST body. `authorized` is whether the HTTP layer accepted the
  // bearer token (or none is required). Returns the reply to send.
  McpReply handlePost(const std::string& body, bool authorized);

 private:
  std::string name_;
  std::string version_;
  std::string instructions_;
  std::string protocolVersion_ = "2025-06-18";
  std::string authToken_;
  ToolRegistry* tools_ = nullptr;
  ResourceRegistry* resources_ = nullptr;
  PromptRegistry* prompts_ = nullptr;
  EdgeStore* store_ = nullptr;
  bool storeWrites_ = false;
  Logger* logger_ = nullptr;
  uint32_t sessionCounter_ = 0;
};

}  // namespace edge

#endif  // EDGELLM_MCP_MCPSERVER_H
