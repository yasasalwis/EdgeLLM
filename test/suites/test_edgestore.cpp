// Tests for the built-in EdgeStore KV datastore, including caps and persistence.
#include "../../src/hal/MemorySecretStore.h"
#include "../../src/mcp/EdgeStore.h"
#include "../framework/edge_test.h"

using namespace edge;

TEST(edgestore_set_get_has) {
  EdgeStore s;
  CHECK(s.set("temp", "21.5").isOk());
  CHECK(s.has("temp"));
  CHECK_STR_EQ(s.get("temp").value(), "21.5");
  CHECK_EQ(s.size(), static_cast<size_t>(1));
}

TEST(edgestore_missing_key) {
  EdgeStore s;
  CHECK(!s.get("nope").isOk());
  CHECK_EQ(s.get("nope").error(), Error::NotFound);
}

TEST(edgestore_remove_is_idempotent) {
  EdgeStore s;
  s.set("a", "1");
  CHECK(s.remove("a").isOk());
  CHECK(!s.has("a"));
  CHECK(s.remove("a").isOk());  // no-op
}

TEST(edgestore_enforces_max_keys) {
  EdgeStore s(2);
  CHECK(s.set("a", "1").isOk());
  CHECK(s.set("b", "2").isOk());
  CHECK_EQ(s.set("c", "3").error(), Error::Capacity);
  CHECK(s.set("a", "updated").isOk());  // overwrite existing is fine
}

TEST(edgestore_enforces_max_value_len) {
  EdgeStore s(32, 4);
  CHECK_EQ(s.set("k", "12345").error(), Error::Capacity);
  CHECK(s.set("k", "1234").isOk());
}

TEST(edgestore_rejects_invalid_keys) {
  EdgeStore s;
  CHECK_EQ(s.set("", "x").error(), Error::InvalidArgument);
  CHECK_EQ(s.set("has\nnewline", "x").error(), Error::InvalidArgument);
}

TEST(edgestore_persists_and_reloads) {
  MemorySecretStore backend;
  {
    EdgeStore s;
    s.setPersistence(&backend);
    s.set("a", "1");
    s.set("b", "2");
  }
  // A fresh store on the same backend recovers the data via the manifest.
  EdgeStore reloaded;
  reloaded.setPersistence(&backend);
  CHECK_EQ(reloaded.size(), static_cast<size_t>(2));
  CHECK_STR_EQ(reloaded.get("a").value(), "1");
  CHECK_STR_EQ(reloaded.get("b").value(), "2");
}

TEST(edgestore_persisted_delete_survives_reload) {
  MemorySecretStore backend;
  {
    EdgeStore s;
    s.setPersistence(&backend);
    s.set("a", "1");
    s.set("b", "2");
    s.remove("a");
  }
  EdgeStore reloaded;
  reloaded.setPersistence(&backend);
  CHECK(!reloaded.has("a"));
  CHECK(reloaded.has("b"));
}

TEST(edgestore_rejects_reserved_manifest_key) {
  EdgeStore s;
  // A user must not be able to clobber the persistence manifest.
  CHECK_EQ(s.set("__edgestore_manifest__", "x").error(), Error::InvalidArgument);
  CHECK(!s.has("__edgestore_manifest__"));
}
