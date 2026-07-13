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

namespace {
uint32_t edgestoreFakeNow = 0;
uint32_t edgestoreFakeClock() { return edgestoreFakeNow; }
}  // namespace

TEST(edgestore_onchange_hook_fires_on_set_and_remove) {
  EdgeStore s;
  std::vector<std::pair<std::string, bool>> events;
  s.setOnChange([&](const std::string& key, bool removed) { events.emplace_back(key, removed); });

  s.set("led", "on");
  s.set("led", "off");  // update fires too
  s.remove("led");
  s.remove("led");  // idempotent no-op: no event

  CHECK_EQ(events.size(), static_cast<size_t>(3));
  CHECK_STR_EQ(events[0].first, "led");
  CHECK(!events[0].second);
  CHECK(!events[1].second);
  CHECK(events[2].second);  // removal
}

TEST(edgestore_ttl_requires_clock) {
  EdgeStore s;
  CHECK_EQ(s.setWithTtl("k", "v", 100).error(), Error::InvalidState);
}

TEST(edgestore_ttl_entry_expires_lazily) {
  EdgeStore s;
  edgestoreFakeNow = 1000;
  s.setClock(edgestoreFakeClock);
  std::vector<std::pair<std::string, bool>> events;
  s.setOnChange([&](const std::string& key, bool removed) { events.emplace_back(key, removed); });

  CHECK(s.setWithTtl("reading", "21.5", 500).isOk());
  CHECK(s.has("reading"));
  CHECK_STR_EQ(s.get("reading").value(), "21.5");
  CHECK_EQ(s.size(), static_cast<size_t>(1));

  edgestoreFakeNow = 1499;  // still alive
  CHECK(s.has("reading"));

  edgestoreFakeNow = 1500;  // deadline reached
  CHECK(!s.has("reading"));
  CHECK_EQ(s.keys().size(), static_cast<size_t>(0));
  CHECK_EQ(s.size(), static_cast<size_t>(0));
  CHECK_EQ(s.get("reading").error(), Error::NotFound);

  // set + expiry-removal events
  CHECK_EQ(events.size(), static_cast<size_t>(2));
  CHECK(events[1].second);
}

TEST(edgestore_ttl_entries_never_persist) {
  MemorySecretStore backend;
  EdgeStore s;
  edgestoreFakeNow = 0;
  s.setClock(edgestoreFakeClock);
  s.setPersistence(&backend);

  CHECK(s.set("durable", "1").isOk());
  CHECK(s.setWithTtl("ephemeral", "2", 1000).isOk());
  CHECK(backend.get("durable").isOk());
  CHECK(!backend.get("ephemeral").isOk());  // TTL entries stay RAM-only

  // Overwriting a durable key with a TTL entry drops the persisted copy so a
  // reboot can't resurrect a stale value.
  CHECK(s.setWithTtl("durable", "3", 1000).isOk());
  CHECK(!backend.get("durable").isOk());

  // A plain set promotes a TTL entry back to durable.
  CHECK(s.set("ephemeral", "4").isOk());
  CHECK(backend.get("ephemeral").isOk());
  edgestoreFakeNow = 5000;
  CHECK(s.has("ephemeral"));  // no longer expires
}
