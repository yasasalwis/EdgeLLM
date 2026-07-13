// EdgeLLM — scripted IConnection for native transport tests.
// Plays back a fixed `toSend` buffer as the "server" response and captures
// everything the client writes. Supports byte-drip (chunkSize), connection
// close after drain, connect failure injection, and a stall mode for testing
// read timeouts.
#ifndef EDGELLM_TEST_FAKECONNECTION_H
#define EDGELLM_TEST_FAKECONNECTION_H

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/transport/IConnection.h"

namespace edgetest {

class FakeConnection : public edge::IConnection {
 public:
  std::string toSend;    // bytes the fake server returns to the client
  std::string written;   // captures bytes the client sent (across all connects)
  size_t chunkSize = 0;  // 0 = give everything available per read; else cap per read
  bool closeWhenDrained = true;
  bool stall = false;  // when true, reads always return 0 and stay connected
  edge::Status connectResult = edge::Status::ok();

  // Optional multi-response script: each connect() serves the next entry (the
  // last entry repeats). Used to test the multi-round-trip agent loop.
  std::vector<std::string> responses;

  // Keep-alive server mode: a new request written after the current response
  // drained serves the next script entry on the SAME connection (no reconnect),
  // like a real HTTP/1.1 persistent connection.
  bool keepAliveServer = false;

  // Stale-socket mode: once the response is drained, reads fail (-1) but
  // connected() stays true — models a server that closed an idle keep-alive
  // connection without the client noticing yet.
  bool staleAfterDrain = false;

  int connectCount = 0;  // how many times connect() was called
  int stopCount = 0;     // how many times stop() was called

  // When > 0, that many leading connect() attempts fail with ConnectFailed
  // before connects start succeeding — for testing transient-failure retries.
  int failFirstConnects = 0;

  edge::Status connect(const char* host, uint16_t port) override {
    (void)host;
    (void)port;
    if (!connectResult) return connectResult;
    if (failFirstConnects > 0) {
      --failFirstConnects;
      return edge::Status::fail(edge::Error::ConnectFailed);
    }
    ++connectCount;
    if (!responses.empty()) {
      toSend = responses[responseIdx_ < responses.size() ? responseIdx_ : responses.size() - 1];
      ++responseIdx_;
      readPos_ = 0;
    }
    connected_ = true;
    return edge::Status::ok();
  }

  bool connected() override { return connected_; }

  int write(const uint8_t* data, size_t len) override {
    // Keep-alive server: a fresh request after the previous response was fully
    // consumed makes the next scripted response available without a reconnect.
    if (keepAliveServer && !responses.empty() && readPos_ >= toSend.size() &&
        responseIdx_ < responses.size()) {
      toSend = responses[responseIdx_];
      ++responseIdx_;
      readPos_ = 0;
    }
    written.append(reinterpret_cast<const char*>(data), len);
    return static_cast<int>(len);
  }

  int available() override {
    if (stall) return 0;
    return static_cast<int>(toSend.size() - readPos_);
  }

  int read(uint8_t* buf, size_t len) override {
    if (stall) return 0;
    if (readPos_ >= toSend.size()) {
      if (staleAfterDrain) return -1;  // reads fail but connected() stays true
      if (closeWhenDrained) {
        connected_ = false;
        return -1;
      }
      return 0;
    }
    size_t remaining = toSend.size() - readPos_;
    size_t take = std::min(len, remaining);
    if (chunkSize > 0) take = std::min(take, chunkSize);
    std::memcpy(buf, toSend.data() + readPos_, take);
    readPos_ += take;
    return static_cast<int>(take);
  }

  void stop() override {
    if (connected_) ++stopCount;
    connected_ = false;
  }

 private:
  bool connected_ = false;
  size_t readPos_ = 0;
  size_t responseIdx_ = 0;
};

// Auto-incrementing fake clock for HttpClient timeout tests. Each call advances
// by `step` ms so a synchronous spin loop is guaranteed to reach the timeout.
inline uint32_t& fakeClockStep() {
  static uint32_t s = 1000;
  return s;
}
inline uint32_t fakeClockNow() {
  static uint32_t t = 0;
  t += fakeClockStep();
  return t;
}

}  // namespace edgetest

#endif  // EDGELLM_TEST_FAKECONNECTION_H
