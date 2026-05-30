// Tests for the incremental chunked transfer-encoding decoder.
#include "../../src/transport/ChunkedDecoder.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
// Decodes `input` one byte at a time to exercise the state machine's
// resumability, mirroring how bytes dribble off a real socket.
std::string decodeByteByByte(const std::string& input, bool& done, Status& status) {
  ChunkedDecoder d;
  std::string out;
  done = false;
  for (size_t i = 0; i < input.size() && !done; ++i) {
    status = d.feed(input.data() + i, 1, out, done);
    if (!status.isOk()) return out;
  }
  return out;
}
}  // namespace

TEST(chunked_single_chunk) {
  ChunkedDecoder d;
  std::string out;
  bool done = false;
  std::string in = "5\r\nhello\r\n0\r\n\r\n";
  Status s = d.feed(in.data(), in.size(), out, done);
  CHECK(s.isOk());
  CHECK(done);
  CHECK_STR_EQ(out, "hello");
}

TEST(chunked_multiple_chunks) {
  ChunkedDecoder d;
  std::string out;
  bool done = false;
  std::string in = "5\r\nhello\r\n5\r\nworld\r\n0\r\n\r\n";
  Status s = d.feed(in.data(), in.size(), out, done);
  CHECK(s.isOk());
  CHECK(done);
  CHECK_STR_EQ(out, "helloworld");
}

TEST(chunked_byte_by_byte_matches) {
  bool done = false;
  Status s = Status::ok();
  std::string out = decodeByteByByte("3\r\nabc\r\n4\r\ndefg\r\n0\r\n\r\n", done, s);
  CHECK(s.isOk());
  CHECK(done);
  CHECK_STR_EQ(out, "abcdefg");
}

TEST(chunked_hex_size_above_nine) {
  ChunkedDecoder d;
  std::string out;
  bool done = false;
  // 0x10 = 16 bytes.
  std::string in = "10\r\n0123456789abcdef\r\n0\r\n\r\n";
  Status s = d.feed(in.data(), in.size(), out, done);
  CHECK(s.isOk());
  CHECK(done);
  CHECK_STR_EQ(out, "0123456789abcdef");
}

TEST(chunked_extension_is_ignored) {
  ChunkedDecoder d;
  std::string out;
  bool done = false;
  std::string in = "5;name=value\r\nhello\r\n0\r\n\r\n";
  Status s = d.feed(in.data(), in.size(), out, done);
  CHECK(s.isOk());
  CHECK(done);
  CHECK_STR_EQ(out, "hello");
}

TEST(chunked_trailers_are_consumed) {
  ChunkedDecoder d;
  std::string out;
  bool done = false;
  std::string in = "5\r\nhello\r\n0\r\nX-Checksum: abc\r\n\r\n";
  Status s = d.feed(in.data(), in.size(), out, done);
  CHECK(s.isOk());
  CHECK(done);
  CHECK_STR_EQ(out, "hello");
}

TEST(chunked_malformed_size_errors) {
  ChunkedDecoder d;
  std::string out;
  bool done = false;
  std::string in = "zz\r\n";  // not hex
  Status s = d.feed(in.data(), in.size(), out, done);
  CHECK(!s.isOk());
  CHECK_EQ(s.error(), Error::HttpMalformed);
}
