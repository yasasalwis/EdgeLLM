#include "HttpClient.h"

#include <vector>

#include "ChunkedDecoder.h"
#include "HttpRequestBuilder.h"

namespace edge {

namespace {
constexpr size_t kMaxHeaderSection = 8192;  // guard against unbounded headers
constexpr char kCrlfCrlf[] = "\r\n\r\n";

std::string trim(const std::string& s) {
  size_t b = 0, e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t'))
    ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r'))
    --e;
  return s.substr(b, e - b);
}

bool containsCaseInsensitive(const std::string& haystack, const char* needle) {
  std::string h;
  h.reserve(haystack.size());
  for (char c : haystack)
    h.push_back((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
  std::string n;
  for (const char* p = needle; *p; ++p)
    n.push_back(*p);
  return h.find(n) != std::string::npos;
}

// Parses the header block (everything before the blank line) into `out`.
Status parseHeaderBlock(const std::string& block, HttpResponse& out) {
  size_t pos = block.find("\r\n");
  if (pos == std::string::npos) return Status::fail(Error::HttpMalformed);

  // Status line: HTTP/x.y <code> <reason>
  const std::string statusLine = block.substr(0, pos);
  const size_t sp1 = statusLine.find(' ');
  if (sp1 == std::string::npos) return Status::fail(Error::HttpMalformed);
  const size_t sp2 = statusLine.find(' ', sp1 + 1);
  const std::string codeStr =
      statusLine.substr(sp1 + 1, (sp2 == std::string::npos ? statusLine.size() : sp2) - sp1 - 1);
  if (codeStr.size() < 3) return Status::fail(Error::HttpMalformed);
  int code = 0;
  for (char c : codeStr) {
    if (c < '0' || c > '9') return Status::fail(Error::HttpMalformed);
    code = code * 10 + (c - '0');
  }
  out.status = code;
  out.reason = (sp2 == std::string::npos) ? "" : trim(statusLine.substr(sp2 + 1));

  // Header lines.
  size_t lineStart = pos + 2;
  while (lineStart < block.size()) {
    size_t lineEnd = block.find("\r\n", lineStart);
    if (lineEnd == std::string::npos) lineEnd = block.size();
    const std::string line = block.substr(lineStart, lineEnd - lineStart);
    lineStart = lineEnd + 2;
    if (line.empty()) break;
    const size_t colon = line.find(':');
    if (colon == std::string::npos) continue;  // tolerate a stray malformed line
    out.headers.emplace_back(trim(line.substr(0, colon)), trim(line.substr(colon + 1)));
  }
  return Status::ok();
}
}  // namespace

HttpClient::HttpClient(IConnection& conn, Logger* logger) : conn_(conn), logger_(logger) {}

bool HttpClient::timedOut(uint32_t startMs) const {
  if (!clock_) return false;
  return (now() - startMs) >= timeoutMs_;
}

Status HttpClient::writeAll(const std::string& data, uint32_t startMs) {
  size_t off = 0;
  while (off < data.size()) {
    const int n =
        conn_.write(reinterpret_cast<const uint8_t*>(data.data()) + off, data.size() - off);
    if (n < 0) return Status::fail(Error::WriteFailed);
    if (n == 0) {
      if (timedOut(startMs)) return Status::fail(Error::Timeout);
      continue;
    }
    off += static_cast<size_t>(n);
  }
  return Status::ok();
}

Status HttpClient::readHeaders(std::string& leftoverBody, HttpResponse& out, uint32_t startMs) {
  std::vector<char> buf(recvBufferSize_);
  // Start from any bytes a pipelining server delivered with the previous
  // response; they are the head of this one.
  std::string acc;
  acc.swap(pending_);
  if (!acc.empty()) sawResponseBytes_ = true;
  for (;;) {
    const size_t end = acc.find(kCrlfCrlf);
    if (end != std::string::npos) {
      Status s = parseHeaderBlock(acc.substr(0, end + 2), out);
      if (!s) return s;
      leftoverBody = acc.substr(end + 4);
      return Status::ok();
    }
    if (acc.size() > kMaxHeaderSection) return Status::fail(Error::HttpMalformed);

    const int n = conn_.read(reinterpret_cast<uint8_t*>(buf.data()), buf.size());
    if (n > 0) {
      sawResponseBytes_ = true;
      acc.append(buf.data(), static_cast<size_t>(n));
    } else if (n < 0) {
      return Status::fail(Error::HttpMalformed);  // closed before headers completed
    } else {
      if (!conn_.connected() && conn_.available() <= 0) return Status::fail(Error::HttpMalformed);
      if (timedOut(startMs)) return Status::fail(Error::Timeout);
    }
  }
}

Status HttpClient::readBody(const std::string& initial, HttpResponse& out, uint32_t startMs) {
  const std::string* te = out.header("Transfer-Encoding");
  const std::string* cl = out.header("Content-Length");
  std::vector<char> buf(recvBufferSize_);

  if (te != nullptr && containsCaseInsensitive(*te, "chunked")) {
    ChunkedDecoder decoder;
    bool done = false;
    Status s = decoder.feed(initial.data(), initial.size(), out.body, done);
    if (!s) return s;
    while (!done) {
      if (out.body.size() > maxResponseBody_) return Status::fail(Error::HttpBodyTooLarge);
      const int n = conn_.read(reinterpret_cast<uint8_t*>(buf.data()), buf.size());
      if (n > 0) {
        s = decoder.feed(buf.data(), static_cast<size_t>(n), out.body, done);
        if (!s) return s;
      } else if (n < 0) {
        return Status::fail(Error::ConnectionClosed);  // truncated chunked body
      } else {
        if (timedOut(startMs)) return Status::fail(Error::Timeout);
        if (!conn_.connected() && conn_.available() <= 0)
          return Status::fail(Error::ConnectionClosed);
      }
    }
    if (out.body.size() > maxResponseBody_) return Status::fail(Error::HttpBodyTooLarge);
    return Status::ok();
  }

  if (cl != nullptr) {
    size_t want = 0;
    for (char c : *cl) {
      if (c >= '0' && c <= '9') want = want * 10 + static_cast<size_t>(c - '0');
    }
    if (want > maxResponseBody_) return Status::fail(Error::HttpBodyTooLarge);
    out.body = initial;
    if (out.body.size() > want) {
      // Bytes past this body belong to the next response — keep them.
      pending_ = out.body.substr(want);
      out.body.resize(want);
    }
    while (out.body.size() < want) {
      // Never read past this response's body: on a keep-alive connection any
      // extra bytes would belong to the next response and must stay unread.
      const size_t room = want - out.body.size();
      const size_t take = room < buf.size() ? room : buf.size();
      const int n = conn_.read(reinterpret_cast<uint8_t*>(buf.data()), take);
      if (n > 0) {
        out.body.append(buf.data(), static_cast<size_t>(n));
        if (out.body.size() > want) out.body.resize(want);
      } else if (n < 0) {
        return Status::fail(Error::ConnectionClosed);  // short read
      } else {
        if (timedOut(startMs)) return Status::fail(Error::Timeout);
        if (!conn_.connected() && conn_.available() <= 0)
          return Status::fail(Error::ConnectionClosed);
      }
    }
    return Status::ok();
  }

  // No framing headers: read until the server closes the connection.
  out.body = initial;
  for (;;) {
    if (out.body.size() > maxResponseBody_) return Status::fail(Error::HttpBodyTooLarge);
    const int n = conn_.read(reinterpret_cast<uint8_t*>(buf.data()), buf.size());
    if (n > 0) {
      out.body.append(buf.data(), static_cast<size_t>(n));
    } else if (n < 0) {
      return Status::ok();  // clean close marks end of body
    } else {
      if (!conn_.connected() && conn_.available() <= 0) return Status::ok();
      if (timedOut(startMs)) return Status::fail(Error::Timeout);
    }
  }
}

Status HttpClient::send(const HttpRequest& req, HttpResponse& out) {
  const bool wasOpen = conn_.connected();
  Status s = sendOnce(req, out);

  // Stale reused connection: the server closed it between requests and we
  // noticed only after writing. No response bytes were received, so retrying
  // once on a fresh connection is safe (RFC 7230 §6.3.1).
  const bool staleError = s.error() == Error::WriteFailed || s.error() == Error::ConnectionClosed ||
                          s.error() == Error::HttpMalformed;
  if (!s.isOk() && wasOpen && !sawResponseBytes_ && staleError) {
    if (logger_) logger_->info("http: reused connection was stale, retrying once");
    conn_.stop();
    out = HttpResponse();
    s = sendOnce(req, out);
  }
  return s;
}

Status HttpClient::sendOnce(const HttpRequest& req, HttpResponse& out) {
  const uint32_t start = now();
  sawResponseBytes_ = false;
  reusable_ = false;

  if (!conn_.connected()) {
    // A fresh connection starts a fresh byte stream: leftover bytes from a
    // previous connection must never prefix this one's response.
    pending_.clear();
    Status s = conn_.connect(req.host.c_str(), req.port);
    if (!s) {
      if (logger_) logger_->warn(std::string("http connect failed: ") + s.message());
      return s;
    }
  }

  Status s = writeAll(serializeRequest(req, keepAlive_), start);
  if (!s) return s;

  std::string leftover;
  s = readHeaders(leftover, out, start);
  if (!s) return s;

  // 1xx, 204 and 304 carry no body by definition.
  const bool hasBody =
      !(out.status == 204 || out.status == 304 || (out.status >= 100 && out.status < 200));
  if (hasBody) {
    s = readBody(leftover, out, start);
    if (!s) return s;
  }

  if (keepAlive_) {
    // The socket can serve another request only if the server did not announce
    // a close, the body had explicit framing (an until-close body consumed the
    // connection), and the socket survived.
    const std::string* connHdr = out.header("Connection");
    const bool respClose = connHdr != nullptr && containsCaseInsensitive(*connHdr, "close");
    const std::string* te = out.header("Transfer-Encoding");
    const bool framed = !hasBody || out.header("Content-Length") != nullptr ||
                        (te != nullptr && containsCaseInsensitive(*te, "chunked"));
    reusable_ = !respClose && framed && conn_.connected();
    if (!reusable_) conn_.stop();
  }
  return Status::ok();
}

Status HttpClient::sendStream(const HttpRequest& req, HttpResponse& outHeaders,
                              const BodyChunkFn& onChunk) {
  const uint32_t start = now();

  if (!conn_.connected()) {
    Status s = conn_.connect(req.host.c_str(), req.port);
    if (!s) {
      if (logger_) logger_->warn(std::string("http connect failed: ") + s.message());
      return s;
    }
  }

  Status s = writeAll(serializeRequest(req), start);
  if (!s) return s;

  std::string leftover;
  s = readHeaders(leftover, outHeaders, start);
  if (!s) return s;

  if (outHeaders.status == 204 || outHeaders.status == 304 ||
      (outHeaders.status >= 100 && outHeaders.status < 200)) {
    return Status::ok();
  }

  const std::string* te = outHeaders.header("Transfer-Encoding");
  const bool chunked = te != nullptr && containsCaseInsensitive(*te, "chunked");
  std::vector<char> buf(recvBufferSize_);
  size_t total = 0;

  // Emits bytes to the caller while enforcing the size cap. Returns false to
  // stop (either cap exceeded -> aborted, or caller asked to stop).
  bool overflow = false;
  auto emit = [&](const char* data, size_t len) -> bool {
    if (len == 0) return true;
    total += len;
    if (total > maxResponseBody_) {
      overflow = true;
      return false;
    }
    return onChunk(data, len);  // false here means the caller asked to stop
  };

  if (chunked) {
    ChunkedDecoder decoder;
    bool done = false;
    std::string decoded;
    s = decoder.feed(leftover.data(), leftover.size(), decoded, done);
    if (!s) return s;
    if (!emit(decoded.data(), decoded.size()))
      return overflow ? Status::fail(Error::HttpBodyTooLarge) : Status::ok();
    while (!done) {
      const int n = conn_.read(reinterpret_cast<uint8_t*>(buf.data()), buf.size());
      if (n > 0) {
        decoded.clear();
        s = decoder.feed(buf.data(), static_cast<size_t>(n), decoded, done);
        if (!s) return s;
        if (!emit(decoded.data(), decoded.size()))
          return overflow ? Status::fail(Error::HttpBodyTooLarge) : Status::ok();
      } else if (n < 0) {
        return Status::fail(Error::ConnectionClosed);
      } else {
        if (timedOut(start)) return Status::fail(Error::Timeout);
        if (!conn_.connected() && conn_.available() <= 0)
          return Status::fail(Error::ConnectionClosed);
      }
    }
    return Status::ok();
  }

  // Non-chunked: stream raw bytes (Content-Length or until close) straight
  // through. We don't strictly enforce Content-Length here because streaming
  // callers consume deltas and stop on their own sentinel.
  if (!emit(leftover.data(), leftover.size()))
    return overflow ? Status::fail(Error::HttpBodyTooLarge) : Status::ok();
  for (;;) {
    const int n = conn_.read(reinterpret_cast<uint8_t*>(buf.data()), buf.size());
    if (n > 0) {
      if (!emit(buf.data(), static_cast<size_t>(n)))
        return overflow ? Status::fail(Error::HttpBodyTooLarge) : Status::ok();
    } else if (n < 0) {
      return Status::ok();  // clean close ends the stream
    } else {
      if (!conn_.connected() && conn_.available() <= 0) return Status::ok();
      if (timedOut(start)) return Status::fail(Error::Timeout);
    }
  }
}

}  // namespace edge
