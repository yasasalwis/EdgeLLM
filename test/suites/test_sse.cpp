// Tests for the incremental SSE parser.
#include <vector>

#include "../../src/transport/SseParser.h"
#include "../framework/edge_test.h"

using namespace edge;

namespace {
// Collects events emitted during a feed for assertion.
std::vector<SseEvent> collect(SseParser& p, const std::string& chunk) {
  std::vector<SseEvent> out;
  p.feed(chunk.data(), chunk.size(), [&](const SseEvent& e) { out.push_back(e); });
  return out;
}
}  // namespace

TEST(sse_single_event) {
  SseParser p;
  auto ev = collect(p, "data: hello\n\n");
  CHECK_EQ(ev.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(ev[0].event, "message");
  CHECK_STR_EQ(ev[0].data, "hello");
}

TEST(sse_multiline_data_joined_with_newline) {
  SseParser p;
  auto ev = collect(p, "data: a\ndata: b\n\n");
  CHECK_EQ(ev.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(ev[0].data, "a\nb");
}

TEST(sse_event_type_and_id) {
  SseParser p;
  auto ev = collect(p, "event: ping\nid: 42\ndata: x\n\n");
  CHECK_EQ(ev.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(ev[0].event, "ping");
  CHECK_STR_EQ(ev[0].id, "42");
  CHECK_STR_EQ(ev[0].data, "x");
}

TEST(sse_comment_lines_ignored) {
  SseParser p;
  auto ev = collect(p, ": this is a comment\ndata: y\n\n");
  CHECK_EQ(ev.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(ev[0].data, "y");
}

TEST(sse_blank_event_with_no_data_emits_nothing) {
  SseParser p;
  auto ev = collect(p, "\n\n");
  CHECK_EQ(ev.size(), static_cast<size_t>(0));
}

TEST(sse_handles_crlf_line_endings) {
  SseParser p;
  auto ev = collect(p, "data: z\r\n\r\n");
  CHECK_EQ(ev.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(ev[0].data, "z");
}

TEST(sse_reassembles_across_feeds) {
  SseParser p;
  std::vector<SseEvent> all;
  auto sink = [&](const SseEvent& e) { all.push_back(e); };
  std::string a = "data: hel";
  std::string b = "lo\n\ndata: world\n\n";
  p.feed(a.data(), a.size(), sink);
  CHECK_EQ(all.size(), static_cast<size_t>(0));  // not complete yet
  p.feed(b.data(), b.size(), sink);
  CHECK_EQ(all.size(), static_cast<size_t>(2));
  CHECK_STR_EQ(all[0].data, "hello");
  CHECK_STR_EQ(all[1].data, "world");
}

TEST(sse_id_persists_across_events) {
  SseParser p;
  std::vector<SseEvent> all;
  auto sink = [&](const SseEvent& e) { all.push_back(e); };
  std::string s = "id: 1\ndata: a\n\ndata: b\n\n";
  p.feed(s.data(), s.size(), sink);
  CHECK_EQ(all.size(), static_cast<size_t>(2));
  CHECK_STR_EQ(all[0].id, "1");
  CHECK_STR_EQ(all[1].id, "1");  // sticky per spec
}

TEST(sse_done_sentinel_is_passed_through_as_data) {
  SseParser p;
  auto ev = collect(p, "data: [DONE]\n\n");
  CHECK_EQ(ev.size(), static_cast<size_t>(1));
  CHECK_STR_EQ(ev[0].data, "[DONE]");
}
