#include "ChunkedDecoder.h"

namespace edge {

namespace {
// Returns 0-15 for a hex digit, or -1 if not a hex digit.
int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
}  // namespace

void ChunkedDecoder::reset() {
  state_ = State::Size;
  remaining_ = 0;
  chunkSize_ = 0;
  sawSizeDigit_ = false;
  trailerLineLen_ = 0;
}

Status ChunkedDecoder::feed(const char* data, size_t len, std::string& out, bool& done) {
  for (size_t i = 0; i < len; ++i) {
    const char c = data[i];
    switch (state_) {
      case State::Size: {
        const int hv = hexValue(c);
        if (hv >= 0) {
          chunkSize_ = (chunkSize_ << 4) | static_cast<size_t>(hv);
          sawSizeDigit_ = true;
        } else if (c == ';') {
          state_ = State::SizeExt;  // chunk extension follows; ignore it
        } else if (c == '\r') {
          // wait for the LF
        } else if (c == '\n') {
          if (!sawSizeDigit_) return Status::fail(Error::HttpMalformed);
          if (chunkSize_ == 0) {
            state_ = State::Trailer;
            trailerLineLen_ = 0;
          } else {
            remaining_ = chunkSize_;
            state_ = State::Data;
          }
          chunkSize_ = 0;
          sawSizeDigit_ = false;
        } else {
          return Status::fail(Error::HttpMalformed);
        }
        break;
      }

      case State::SizeExt: {
        if (c == '\n') {
          if (chunkSize_ == 0 && !sawSizeDigit_) return Status::fail(Error::HttpMalformed);
          if (chunkSize_ == 0) {
            state_ = State::Trailer;
            trailerLineLen_ = 0;
          } else {
            remaining_ = chunkSize_;
            state_ = State::Data;
          }
          chunkSize_ = 0;
          sawSizeDigit_ = false;
        }
        // All other extension bytes are ignored until the LF.
        break;
      }

      case State::Data: {
        // Copy as much of the rest of this input as fits in the current chunk.
        const size_t avail = len - i;
        const size_t take = avail < remaining_ ? avail : remaining_;
        out.append(data + i, take);
        remaining_ -= take;
        i += take - 1;  // -1 because the for-loop will ++i
        if (remaining_ == 0) state_ = State::DataCr;
        break;
      }

      case State::DataCr: {
        // Tolerate a bare LF, but normally expect CR then LF.
        if (c == '\n') {
          state_ = State::Size;
        } else if (c == '\r') {
          state_ = State::DataLf;
        } else {
          return Status::fail(Error::HttpMalformed);
        }
        break;
      }

      case State::DataLf: {
        if (c != '\n') return Status::fail(Error::HttpMalformed);
        state_ = State::Size;
        break;
      }

      case State::Trailer: {
        if (c == '\n') {
          if (trailerLineLen_ == 0) {
            state_ = State::Done;
            done = true;
            return Status::ok();
          }
          trailerLineLen_ = 0;  // end of a non-empty trailer line
        } else if (c != '\r') {
          ++trailerLineLen_;
        }
        break;
      }

      case State::Done:
        done = true;
        return Status::ok();
    }
  }

  done = (state_ == State::Done);
  return Status::ok();
}

}  // namespace edge
