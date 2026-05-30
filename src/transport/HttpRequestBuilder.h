// EdgeLLM — serialize an HttpRequest into raw HTTP/1.1 wire bytes.
// Arduino-independent and pure (no I/O), which makes it fully unit-testable on
// the host. Automatically supplies Host, Content-Length and a default
// Connection header when the caller has not set them.
#ifndef EDGELLM_TRANSPORT_HTTPREQUESTBUILDER_H
#define EDGELLM_TRANSPORT_HTTPREQUESTBUILDER_H

#include <string>

#include "HttpTypes.h"

namespace edge {

// Returns the complete request (request line + headers + CRLF + body) ready to
// write to a socket. Pure function: identical input always yields identical
// output, so it can be golden-tested.
std::string serializeRequest(const HttpRequest& req);

}  // namespace edge

#endif  // EDGELLM_TRANSPORT_HTTPREQUESTBUILDER_H
