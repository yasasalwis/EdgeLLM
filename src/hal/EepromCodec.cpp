#include "EepromCodec.h"

namespace edge {

namespace {
constexpr uint8_t kMagic[4] = {'E', 'L', 'K', 'V'};
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 4 + 1 + 2;  // magic + version + count
constexpr size_t kMaxKeyLen = 255;
constexpr size_t kMaxValLen = 65535;

void putU16(std::vector<uint8_t>& out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v & 0xFF));
  out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
uint16_t getU16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}
}  // namespace

bool encodeKv(const std::map<std::string, std::string>& kv, std::vector<uint8_t>& out,
              size_t maxBytes) {
  out.clear();
  if (kv.size() > 0xFFFF) return false;

  // Reject oversized fields up front.
  for (const auto& e : kv) {
    if (e.first.size() > kMaxKeyLen || e.second.size() > kMaxValLen) return false;
  }

  std::vector<uint8_t> buf;
  buf.reserve(kHeaderSize);
  buf.insert(buf.end(), kMagic, kMagic + 4);
  buf.push_back(kVersion);
  putU16(buf, static_cast<uint16_t>(kv.size()));

  for (const auto& e : kv) {
    buf.push_back(static_cast<uint8_t>(e.first.size()));
    buf.insert(buf.end(), e.first.begin(), e.first.end());
    putU16(buf, static_cast<uint16_t>(e.second.size()));
    buf.insert(buf.end(), e.second.begin(), e.second.end());
    if (buf.size() > maxBytes) return false;
  }
  if (buf.size() > maxBytes) return false;

  out.swap(buf);
  return true;
}

bool decodeKv(const uint8_t* data, size_t len, std::map<std::string, std::string>& out) {
  out.clear();
  if (len < kHeaderSize) return false;
  if (data[0] != kMagic[0] || data[1] != kMagic[1] || data[2] != kMagic[2] ||
      data[3] != kMagic[3]) {
    return false;
  }
  if (data[4] != kVersion) return false;

  const uint16_t count = getU16(data + 5);
  size_t pos = kHeaderSize;
  for (uint16_t i = 0; i < count; ++i) {
    if (pos + 1 > len) return false;
    const size_t keyLen = data[pos++];
    if (pos + keyLen + 2 > len) return false;
    std::string key(reinterpret_cast<const char*>(data + pos), keyLen);
    pos += keyLen;
    const uint16_t valLen = getU16(data + pos);
    pos += 2;
    if (pos + valLen > len) return false;
    std::string val(reinterpret_cast<const char*>(data + pos), valLen);
    pos += valLen;
    out[key] = val;
  }
  return true;
}

}  // namespace edge
