#include "ResourceRegistry.h"

namespace edge {

Resource& ResourceBuilder::resource() { return registry_->resources_[index_]; }

ResourceBuilder& ResourceBuilder::description(const std::string& text) {
  resource().description = text;
  return *this;
}

ResourceBuilder& ResourceBuilder::mimeType(const std::string& mime) {
  resource().mimeType = mime;
  return *this;
}

ResourceBuilder& ResourceBuilder::blob(bool value) {
  resource().isBlob = value;
  return *this;
}

ResourceBuilder& ResourceBuilder::onRead(
    std::function<Result<std::string>(const std::string&)> handler) {
  resource().onRead = std::move(handler);
  return *this;
}

ResourceBuilder ResourceRegistry::addResource(const std::string& uri, const std::string& name) {
  // Re-registering the same URI redefines it in place (URIs must be unique).
  for (size_t i = 0; i < resources_.size(); ++i) {
    if (resources_[i].uri == uri) {
      resources_[i] = Resource{};
      resources_[i].uri = uri;
      resources_[i].name = name;
      return ResourceBuilder(this, i);
    }
  }
  Resource r;
  r.uri = uri;
  r.name = name;
  resources_.push_back(std::move(r));
  return ResourceBuilder(this, resources_.size() - 1);
}

const Resource* ResourceRegistry::find(const std::string& uri) const {
  for (const auto& r : resources_) {
    if (r.uri == uri) return &r;
  }
  return nullptr;
}

Result<std::string> ResourceRegistry::read(const std::string& uri) const {
  const Resource* r = find(uri);
  if (r == nullptr) return Result<std::string>::fail(Error::NotFound);
  if (!r->onRead) return Result<std::string>::fail(Error::InvalidState);
  return r->onRead(uri);
}

}  // namespace edge
