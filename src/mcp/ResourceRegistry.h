// EdgeLLM — MCP resource registry (read-only data exposed to hosts).
// Arduino-independent. A resource is addressable by URI and produces content on
// read via a handler (e.g. a sensor reading, device status JSON). This is the
// "read" half of the MCP server; mutating operations are tools.
#ifndef EDGELLM_MCP_RESOURCEREGISTRY_H
#define EDGELLM_MCP_RESOURCEREGISTRY_H

#include <functional>
#include <string>
#include <vector>

#include "../core/Result.h"

namespace edge {

struct Resource {
  std::string uri;
  std::string name;
  std::string description;
  std::string mimeType = "text/plain";
  // Produces the resource content. Receives the requested URI (useful when one
  // handler serves a family of URIs).
  std::function<Result<std::string>(const std::string& uri)> onRead;
};

class ResourceRegistry;

class ResourceBuilder {
 public:
  ResourceBuilder& description(const std::string& text);
  ResourceBuilder& mimeType(const std::string& mime);
  ResourceBuilder& onRead(std::function<Result<std::string>(const std::string&)> handler);

 private:
  friend class ResourceRegistry;
  ResourceBuilder(ResourceRegistry* reg, size_t index) : registry_(reg), index_(index) {}
  Resource& resource();
  ResourceRegistry* registry_;
  size_t index_;
};

class ResourceRegistry {
 public:
  ResourceBuilder addResource(const std::string& uri, const std::string& name);

  const std::vector<Resource>& resources() const { return resources_; }
  const Resource* find(const std::string& uri) const;
  size_t size() const { return resources_.size(); }
  bool empty() const { return resources_.empty(); }

  // Reads a resource by URI. Returns NotFound if absent, InvalidState if it has
  // no handler, or the handler's result.
  Result<std::string> read(const std::string& uri) const;

 private:
  friend class ResourceBuilder;
  std::vector<Resource> resources_;
};

}  // namespace edge

#endif  // EDGELLM_MCP_RESOURCEREGISTRY_H
