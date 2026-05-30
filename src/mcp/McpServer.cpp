#include "McpServer.h"

#include <ArduinoJson.h>

namespace edge {

namespace {
// JSON-RPC 2.0 error codes (plus MCP's resource-not-found).
constexpr int kParseError = -32700;
constexpr int kInvalidRequest = -32600;
constexpr int kMethodNotFound = -32601;
constexpr int kInvalidParams = -32602;
constexpr int kResourceNotFound = -32002;
constexpr int kUnauthorized = -32001;

// Constant-time string comparison so bearer-token validation does not leak the
// matching prefix length through timing. (Length is compared first, which is
// standard and acceptable.)
bool constantTimeEquals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  unsigned char diff = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    diff |= static_cast<unsigned char>(a[i]) ^ static_cast<unsigned char>(b[i]);
  }
  return diff == 0;
}

std::string itos(unsigned long v) {
  if (v == 0) return "0";
  char buf[24];
  size_t i = sizeof(buf);
  while (v > 0 && i > 0) {
    buf[--i] = static_cast<char>('0' + (v % 10));
    v /= 10;
  }
  return std::string(buf + i, sizeof(buf) - i);
}

void writeError(JsonObject out, JsonVariantConst id, int code, const char* message) {
  out["jsonrpc"] = "2.0";
  if (id.isNull())
    out["id"] = nullptr;
  else
    out["id"] = id;
  JsonObject e = out["error"].to<JsonObject>();
  e["code"] = code;
  e["message"] = message;
}

// Adds a {content:[{type:text,text}], isError} result to a tools/call response.
void writeToolResult(JsonObject out, const std::string& text, bool isError) {
  JsonObject r = out["result"].to<JsonObject>();
  JsonArray content = r["content"].to<JsonArray>();
  JsonObject c = content.add<JsonObject>();
  c["type"] = "text";
  c["text"] = text;
  r["isError"] = isError;
}

// Adds the kv_set / kv_delete tool definitions to a tools/list array.
void addKvTools(JsonArray arr) {
  {
    JsonObject t = arr.add<JsonObject>();
    t["name"] = "kv_set";
    t["description"] = "Store a value in the device key-value store";
    JsonObject s = t["inputSchema"].to<JsonObject>();
    s["type"] = "object";
    JsonObject props = s["properties"].to<JsonObject>();
    props["key"]["type"] = "string";
    props["value"]["type"] = "string";
    JsonArray req = s["required"].to<JsonArray>();
    req.add("key");
    req.add("value");
    JsonObject ann = t["annotations"].to<JsonObject>();
    ann["readOnlyHint"] = false;
    ann["destructiveHint"] = false;
  }
  {
    JsonObject t = arr.add<JsonObject>();
    t["name"] = "kv_delete";
    t["description"] = "Delete a value from the device key-value store";
    JsonObject s = t["inputSchema"].to<JsonObject>();
    s["type"] = "object";
    s["properties"]["key"]["type"] = "string";
    JsonArray req = s["required"].to<JsonArray>();
    req.add("key");
    JsonObject ann = t["annotations"].to<JsonObject>();
    ann["readOnlyHint"] = false;
    ann["destructiveHint"] = true;
  }
}
}  // namespace

bool McpServer::checkAuth(const std::string& authorizationHeader) const {
  if (!authRequired()) return true;
  const std::string expected = "Bearer " + authToken_;
  return constantTimeEquals(authorizationHeader, expected);
}

McpReply McpServer::handlePost(const std::string& body, bool authorized) {
  McpReply reply;

  if (authRequired() && !authorized) {
    JsonDocument d;
    writeError(d.to<JsonObject>(), JsonVariantConst(), kUnauthorized, "Unauthorized");
    serializeJson(d, reply.body);
    reply.httpStatus = 401;
    return reply;
  }

  JsonDocument reqDoc;
  if (deserializeJson(reqDoc, body)) {
    JsonDocument d;
    writeError(d.to<JsonObject>(), JsonVariantConst(), kParseError, "Parse error");
    serializeJson(d, reply.body);
    reply.httpStatus = 400;
    return reply;
  }

  // Handles a single JSON-RPC request object, writing into `out`. Returns true
  // if `out` holds a response (false for notifications).
  auto processOne = [&](JsonObjectConst req, JsonObject out) -> bool {
    JsonVariantConst id = req["id"];
    const char* method = req["method"];
    const bool isNotification = id.isNull();

    if (!method) {
      if (isNotification) return false;
      writeError(out, id, kInvalidRequest, "Invalid Request");
      return true;
    }
    const std::string m = method;
    JsonObjectConst params = req["params"];

    // Notifications (e.g. notifications/initialized) get no response.
    if (m.rfind("notifications/", 0) == 0) return false;
    if (isNotification) return false;

    out["jsonrpc"] = "2.0";
    out["id"] = id;

    if (m == "initialize") {
      JsonObject r = out["result"].to<JsonObject>();
      const char* pv = params["protocolVersion"];
      r["protocolVersion"] = (pv && *pv) ? std::string(pv) : protocolVersion_;
      JsonObject caps = r["capabilities"].to<JsonObject>();
      if (tools_ || store_) caps["tools"].to<JsonObject>();
      if (resources_ || store_) caps["resources"].to<JsonObject>();
      if (prompts_) caps["prompts"].to<JsonObject>();
      JsonObject si = r["serverInfo"].to<JsonObject>();
      si["name"] = name_;
      si["version"] = version_;
      if (!instructions_.empty()) r["instructions"] = instructions_;
      reply.sessionId = "edgellm-" + itos(++sessionCounter_);
      reply.setSession = true;
      return true;
    }

    if (m == "ping") {
      out["result"].to<JsonObject>();
      return true;
    }

    if (m == "tools/list") {
      JsonObject r = out["result"].to<JsonObject>();
      JsonArray arr = r["tools"].to<JsonArray>();
      if (tools_) {
        for (const Tool& t : tools_->tools()) {
          if (t.mutating && !t.writeAllowed) continue;  // deny-by-default
          JsonObject to = arr.add<JsonObject>();
          to["name"] = t.name;
          to["description"] = t.description;
          JsonObject schema = to["inputSchema"].to<JsonObject>();
          writeToolSchema(t, schema);
          JsonObject ann = to["annotations"].to<JsonObject>();
          ann["readOnlyHint"] = !t.mutating;
          ann["destructiveHint"] = t.mutating;
        }
      }
      if (store_ && storeWrites_) addKvTools(arr);
      return true;
    }

    if (m == "tools/call") {
      const char* name = params["name"];
      if (!name) {
        writeError(out, id, kInvalidParams, "Missing tool name");
        return true;
      }
      const std::string toolName = name;
      std::string argsJson = "{}";
      if (!params["arguments"].isNull()) serializeJson(params["arguments"], argsJson);

      // Built-in KV write tools.
      if (store_ && storeWrites_ && (toolName == "kv_set" || toolName == "kv_delete")) {
        ToolCallArgs a(argsJson);
        if (toolName == "kv_set") {
          if (!a.has("key") || !a.has("value")) {
            writeError(out, id, kInvalidParams, "kv_set requires key and value");
            return true;
          }
          Status s = store_->set(a.getString("key"), a.getString("value"));
          writeToolResult(out, s.isOk() ? "stored" : errorString(s.error()), !s.isOk());
        } else {
          if (!a.has("key")) {
            writeError(out, id, kInvalidParams, "kv_delete requires key");
            return true;
          }
          store_->remove(a.getString("key"));
          writeToolResult(out, "deleted", false);
        }
        return true;
      }

      if (tools_) {
        const Tool* t = tools_->find(toolName);
        if (t != nullptr) {
          if (t->mutating && !t->writeAllowed) {
            writeError(out, id, kInvalidParams, "Tool is a disabled write operation");
            return true;
          }
          Result<ToolResult> r = tools_->dispatch(toolName, argsJson);
          if (r.isOk())
            writeToolResult(out, r.value().content, r.value().isError);
          else
            writeError(out, id, kInvalidParams, errorString(r.error()));
          return true;
        }
      }
      writeError(out, id, kInvalidParams, "Unknown tool");
      return true;
    }

    if (m == "resources/list") {
      JsonObject r = out["result"].to<JsonObject>();
      JsonArray arr = r["resources"].to<JsonArray>();
      if (resources_) {
        for (const Resource& res : resources_->resources()) {
          JsonObject o = arr.add<JsonObject>();
          o["uri"] = res.uri;
          o["name"] = res.name;
          if (!res.description.empty()) o["description"] = res.description;
          o["mimeType"] = res.mimeType;
        }
      }
      if (store_) {
        for (const std::string& key : store_->keys()) {
          JsonObject o = arr.add<JsonObject>();
          o["uri"] = "kv://" + key;
          o["name"] = key;
          o["mimeType"] = "text/plain";
        }
      }
      return true;
    }

    if (m == "resources/templates/list") {
      JsonObject r = out["result"].to<JsonObject>();
      JsonArray arr = r["resourceTemplates"].to<JsonArray>();
      if (store_) {
        JsonObject o = arr.add<JsonObject>();
        o["uriTemplate"] = "kv://{key}";
        o["name"] = "Key-value entry";
        o["mimeType"] = "text/plain";
      }
      return true;
    }

    if (m == "resources/read") {
      const char* uri = params["uri"];
      if (!uri) {
        writeError(out, id, kInvalidParams, "Missing uri");
        return true;
      }
      const std::string u = uri;
      std::string content;
      std::string mime = "text/plain";
      bool found = false;

      if (store_ && u.rfind("kv://", 0) == 0) {
        Result<std::string> r = store_->get(u.substr(5));
        if (r.isOk()) {
          content = r.value();
          found = true;
        }
      } else if (resources_) {
        const Resource* res = resources_->find(u);
        if (res != nullptr) {
          Result<std::string> r = resources_->read(u);
          if (r.isOk()) {
            content = r.value();
            mime = res->mimeType;
            found = true;
          }
        }
      }

      if (!found) {
        writeError(out, id, kResourceNotFound, "Resource not found");
        return true;
      }
      JsonObject r = out["result"].to<JsonObject>();
      JsonArray contents = r["contents"].to<JsonArray>();
      JsonObject c = contents.add<JsonObject>();
      c["uri"] = u;
      c["mimeType"] = mime;
      c["text"] = content;
      return true;
    }

    if (m == "prompts/list") {
      JsonObject r = out["result"].to<JsonObject>();
      JsonArray arr = r["prompts"].to<JsonArray>();
      if (prompts_) {
        for (const Prompt& p : prompts_->prompts()) {
          JsonObject o = arr.add<JsonObject>();
          o["name"] = p.name;
          o["description"] = p.description;
          if (!p.args.empty()) {
            JsonArray as = o["arguments"].to<JsonArray>();
            for (const PromptArg& a : p.args) {
              JsonObject ao = as.add<JsonObject>();
              ao["name"] = a.name;
              ao["description"] = a.description;
              ao["required"] = a.required;
            }
          }
        }
      }
      return true;
    }

    if (m == "prompts/get") {
      const char* name = params["name"];
      if (!name || !prompts_) {
        writeError(out, id, kInvalidParams, "Unknown prompt");
        return true;
      }
      std::string argsJson = "{}";
      if (!params["arguments"].isNull()) serializeJson(params["arguments"], argsJson);
      Result<std::string> r = prompts_->get(name, argsJson);
      if (!r.isOk()) {
        writeError(out, id, kInvalidParams,
                   r.error() == Error::NotFound ? "Unknown prompt" : "Invalid prompt arguments");
        return true;
      }
      JsonObject res = out["result"].to<JsonObject>();
      JsonArray messages = res["messages"].to<JsonArray>();
      JsonObject msg = messages.add<JsonObject>();
      msg["role"] = "user";
      JsonObject c = msg["content"].to<JsonObject>();
      c["type"] = "text";
      c["text"] = r.value();
      return true;
    }

    writeError(out, id, kMethodNotFound, "Method not found");
    return true;
  };

  if (reqDoc.is<JsonArray>()) {
    JsonArrayConst arr = reqDoc.as<JsonArrayConst>();
    if (arr.size() == 0) {
      JsonDocument d;
      writeError(d.to<JsonObject>(), JsonVariantConst(), kInvalidRequest, "Invalid Request");
      serializeJson(d, reply.body);
      reply.httpStatus = 400;
      return reply;
    }
    JsonDocument respDoc;
    JsonArray respArr = respDoc.to<JsonArray>();
    for (JsonObjectConst req : arr) {
      const size_t idx = respArr.size();
      JsonObject o = respArr.add<JsonObject>();
      if (!processOne(req, o)) respArr.remove(idx);
    }
    if (respArr.size() == 0) {
      reply.isNotificationOnly = true;
      reply.httpStatus = 202;
      return reply;
    }
    serializeJson(respDoc, reply.body);
    return reply;
  }

  JsonDocument respDoc;
  JsonObject o = respDoc.to<JsonObject>();
  if (!processOne(reqDoc.as<JsonObjectConst>(), o)) {
    reply.isNotificationOnly = true;
    reply.httpStatus = 202;
    return reply;
  }
  serializeJson(respDoc, reply.body);
  return reply;
}

}  // namespace edge
