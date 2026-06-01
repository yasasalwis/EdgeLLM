// EdgeLLM — incremental HTTP chunked transfer-encoding decoder.
// Arduino-independent and pure. Cloud LLM APIs almost always return
// `Transfer-Encoding: chunked`, so the transport must decode it as bytes
// arrive. This is a byte-at-a-time state machine: feed it whatever came off the
// socket and it appends decoded body bytes to `out`, setting `done` when the
// terminating zero-length chunk has been seen.
#ifndef EDGELLM_TRANSPORT_CHUNKEDDECODER_H
#define EDGELLM_TRANSPORT_CHUNKEDDECODER_H

#include <cstdint>

#include <cstddef>
#include <string>

#include "../core/Result.h"

namespace edge {

class ChunkedDecoder {
 public:
  // Decodes the bytes in [data, data+len). Decoded payload is appended to `out`.
  // `done` is set true once the final 0-length chunk and its trailers have been
  // consumed. Returns Error::HttpMalformed on an invalid chunk header.
  Status feed(const char* data, size_t len, std::string& out, bool& done);

  bool done() const { return state_ == State::Done; }
  void reset();

 private:
  enum class State : uint8_t {
    Size,     // reading the hex chunk-size (and any extension)
    SizeExt,  // skipping a chunk extension until CR
    Data,     // copying chunk-data bytes
    DataCr,   // expecting CR after chunk-data
    DataLf,   // expecting LF after that CR
    Trailer,  // reading trailer lines until a blank line
    Done,     // terminal
  };

  State state_ = State::Size;
  size_t remaining_ = 0;       // bytes left in the current chunk
  size_t chunkSize_ = 0;       // size being accumulated from the hex header
  bool sawSizeDigit_ = false;  // guards against an empty size line
  size_t trailerLineLen_ = 0;  // length of the current trailer line (CR ignored)
};

}  // namespace edge

#endif  // EDGELLM_TRANSPORT_CHUNKEDDECODER_H
