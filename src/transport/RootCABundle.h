// EdgeLLM — default root CA bundle (GENERATED FILE).
//
// This placeholder ships with NO certificates on purpose. Baking a fixed CA
// bundle into a library is an anti-pattern: roots rotate, and a stale bundle
// silently breaks TLS or, worse, tempts users to disable verification.
//
// To populate a real, current bundle (Mozilla's, via curl.se) run:
//
//     python3 tools/gen_ca_bundle.py
//
// That regenerates this file with `kEdgeLLMRootCABundle` set to a PROGMEM PEM
// string and `EDGELLM_HAS_CA_BUNDLE` set to 1. Re-run it periodically (the
// RUNBOOK documents the cadence) to stay current.
//
// Until then, supply a root explicitly with `client.setCACert(pem)` (recommended
// for production: pin the single root your provider uses), or use
// `client.setInsecure()` ONLY for a trusted local endpoint such as Ollama on
// your LAN.
#ifndef EDGELLM_TRANSPORT_ROOTCABUNDLE_H
#define EDGELLM_TRANSPORT_ROOTCABUNDLE_H

#define EDGELLM_HAS_CA_BUNDLE 0

namespace edge {

// nullptr until the generator runs. Consumers must treat nullptr as "no bundle
// available" and fall back to an explicitly provided certificate.
inline const char* defaultCABundle() { return nullptr; }

}  // namespace edge

#endif  // EDGELLM_TRANSPORT_ROOTCABUNDLE_H
