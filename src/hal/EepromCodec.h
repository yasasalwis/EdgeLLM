// EdgeLLM — pure serializer for the EEPROM-backed secret store.
// Arduino-independent so the (fiddly) byte layout can be unit-tested on the
// host; the EepromSecretStore device glue is a thin wrapper over this codec plus
// the Arduino EEPROM API.
//
// Blob layout (little-endian):
//   "ELKV" (4) | version (1) | count (2) | repeated:
//     keyLen (1) | key bytes | valLen (2) | value bytes
#ifndef EDGELLM_HAL_EEPROMCODEC_H
#define EDGELLM_HAL_EEPROMCODEC_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace edge {

// Serializes `kv` into `out`. Returns false (and leaves `out` empty) if the blob
// would exceed `maxBytes`, or if a key/value exceeds the per-field length limits
// (key <= 255 bytes, value <= 65535 bytes).
bool encodeKv(const std::map<std::string, std::string>& kv, std::vector<uint8_t>& out,
              size_t maxBytes);

// Parses a blob produced by encodeKv. Returns false (and clears `out`) if the
// magic/version don't match or the data is truncated/inconsistent.
bool decodeKv(const uint8_t* data, size_t len, std::map<std::string, std::string>& out);

}  // namespace edge

#endif  // EDGELLM_HAL_EEPROMCODEC_H
