// Tests for the in-memory secret store (also the fallback impl on-device).
#include "../../src/hal/MemorySecretStore.h"
#include "../framework/edge_test.h"

using namespace edge;

TEST(secret_set_and_get) {
  MemorySecretStore store;
  CHECK(store.set("anthropic_api_key", "sk-ant-123").isOk());
  auto got = store.get("anthropic_api_key");
  CHECK(got.isOk());
  CHECK_STR_EQ(got.value(), "sk-ant-123");
  CHECK(store.has("anthropic_api_key"));
}

TEST(secret_missing_key_reports_not_found) {
  MemorySecretStore store;
  auto got = store.get("nope");
  CHECK(!got.isOk());
  CHECK_EQ(got.error(), Error::SecretNotFound);
  CHECK(!store.has("nope"));
}

TEST(secret_empty_key_is_invalid) {
  MemorySecretStore store;
  CHECK_EQ(store.set("", "x").error(), Error::InvalidArgument);
}

TEST(secret_overwrite_replaces) {
  MemorySecretStore store;
  store.set("k", "v1");
  store.set("k", "v2");
  CHECK_STR_EQ(store.get("k").value(), "v2");
  CHECK_EQ(store.size(), static_cast<size_t>(1));
}

TEST(secret_remove_and_clear) {
  MemorySecretStore store;
  store.set("a", "1");
  store.set("b", "2");
  CHECK(store.remove("a").isOk());
  CHECK(!store.has("a"));
  CHECK(store.remove("missing").isOk());  // remove is idempotent
  CHECK(store.clear().isOk());
  CHECK_EQ(store.size(), static_cast<size_t>(0));
}
