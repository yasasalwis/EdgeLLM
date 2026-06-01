// Tests for the EEPROM secret-store serializer (the device glue is thin; this
// codec holds the fiddly byte layout, so it's the part worth unit-testing).
#include "../../src/hal/EepromCodec.h"
#include "../framework/edge_test.h"

using namespace edge;

TEST(eeprom_codec_roundtrips) {
  std::map<std::string, std::string> kv{
      {"wifi_ssid", "MyNetwork"}, {"wifi_password", "p@ss w/ spaces"}, {"k", "sk-ant-123"}};
  std::vector<uint8_t> blob;
  CHECK(encodeKv(kv, blob, 1024));

  std::map<std::string, std::string> out;
  CHECK(decodeKv(blob.data(), blob.size(), out));
  CHECK_EQ(out.size(), static_cast<size_t>(3));
  CHECK_STR_EQ(out["wifi_ssid"], "MyNetwork");
  CHECK_STR_EQ(out["wifi_password"], "p@ss w/ spaces");
  CHECK_STR_EQ(out["k"], "sk-ant-123");
}

TEST(eeprom_codec_empty_map_roundtrips) {
  std::map<std::string, std::string> kv;
  std::vector<uint8_t> blob;
  CHECK(encodeKv(kv, blob, 64));
  std::map<std::string, std::string> out;
  CHECK(decodeKv(blob.data(), blob.size(), out));
  CHECK(out.empty());
}

TEST(eeprom_codec_respects_capacity) {
  std::map<std::string, std::string> kv{{"key", "a-value-that-is-too-long"}};
  std::vector<uint8_t> blob;
  CHECK(!encodeKv(kv, blob, 8));  // won't fit in 8 bytes
  CHECK(blob.empty());
}

TEST(eeprom_codec_rejects_bad_magic) {
  const uint8_t garbage[16] = {0};
  std::map<std::string, std::string> out;
  CHECK(!decodeKv(garbage, sizeof(garbage), out));
  CHECK(out.empty());
}

TEST(eeprom_codec_rejects_truncated) {
  std::map<std::string, std::string> kv{{"a", "12345"}};
  std::vector<uint8_t> blob;
  CHECK(encodeKv(kv, blob, 1024));
  std::map<std::string, std::string> out;
  // Chop the last few value bytes — decode must detect the truncation.
  CHECK(!decodeKv(blob.data(), blob.size() - 3, out));
  CHECK(out.empty());
}

TEST(eeprom_codec_handles_binary_values) {
  std::map<std::string, std::string> kv;
  kv["bin"] = std::string("\x01\x00\x02\xff", 4);  // embedded NUL + high byte
  std::vector<uint8_t> blob;
  CHECK(encodeKv(kv, blob, 64));
  std::map<std::string, std::string> out;
  CHECK(decodeKv(blob.data(), blob.size(), out));
  CHECK_EQ(out["bin"].size(), static_cast<size_t>(4));
  CHECK(out["bin"] == std::string("\x01\x00\x02\xff", 4));
}
