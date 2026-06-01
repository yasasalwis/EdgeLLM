// EdgeLLM — version metadata.
// Arduino-independent: safe to include from native builds and on-device.
#ifndef EDGELLM_CORE_VERSION_H
#define EDGELLM_CORE_VERSION_H

#define EDGELLM_VERSION_MAJOR 0
#define EDGELLM_VERSION_MINOR 6
#define EDGELLM_VERSION_PATCH 0
#define EDGELLM_VERSION "0.6.0"

namespace edge {

// Numeric version, e.g. 0.1.0 -> 100. Useful for compile-time feature gates.
constexpr unsigned long kVersionNumber =
    EDGELLM_VERSION_MAJOR * 10000UL + EDGELLM_VERSION_MINOR * 100UL + EDGELLM_VERSION_PATCH;

}  // namespace edge

#endif  // EDGELLM_CORE_VERSION_H
