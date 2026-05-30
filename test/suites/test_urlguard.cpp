// Tests for the SSRF guard used by tools that fetch user-supplied URLs.
#include "../../src/tools/UrlGuard.h"
#include "../framework/edge_test.h"

using namespace edge;

TEST(urlguard_extracts_host) {
  CHECK_STR_EQ(hostFromUrl("https://api.example.com/v1/x?y=1"), "api.example.com");
  CHECK_STR_EQ(hostFromUrl("http://10.0.0.5:8080/path"), "10.0.0.5");
  CHECK_STR_EQ(hostFromUrl("https://user:pass@host.tld/"), "host.tld");
  CHECK_STR_EQ(hostFromUrl("example.org/path"), "example.org");
}

TEST(urlguard_blocks_loopback_and_private) {
  CHECK(isBlockedHost("127.0.0.1"));
  CHECK(isBlockedHost("localhost"));
  CHECK(isBlockedHost("10.1.2.3"));
  CHECK(isBlockedHost("172.16.0.1"));
  CHECK(isBlockedHost("172.31.255.255"));
  CHECK(isBlockedHost("192.168.1.1"));
  CHECK(isBlockedHost("169.254.169.254"));  // cloud metadata
  CHECK(isBlockedHost("printer.local"));
  CHECK(isBlockedHost("::1"));
  CHECK(isBlockedHost("fe80::1"));
}

TEST(urlguard_allows_public_hosts) {
  CHECK(!isBlockedHost("api.anthropic.com"));
  CHECK(!isBlockedHost("8.8.8.8"));
  CHECK(!isBlockedHost("172.32.0.1"));   // just outside the private range
  CHECK(!isBlockedHost("192.169.0.1"));  // not 192.168
}

TEST(urlguard_url_allowed_combines_scheme_and_host) {
  CHECK(isUrlAllowed("https://api.openai.com/v1/chat"));
  CHECK(!isUrlAllowed("http://169.254.169.254/latest/meta-data/"));  // metadata SSRF
  CHECK(!isUrlAllowed("ftp://example.com/file"));                     // non-http scheme
  CHECK(!isUrlAllowed("https://localhost:8080/admin"));
}
