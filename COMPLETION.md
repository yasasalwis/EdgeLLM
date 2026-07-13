# EdgeLLM — Completion Audit & Handoff

This document is the honest, end-of-build audit for EdgeLLM (updated for
v0.7.0). It records what is done, how it was verified, and what remains — so
the next engineer can pick this up cold.

## Verification environment (what "verified" means here)

- ✅ **Native unit tests** — compiled and **executed** on the host with
  `clang++ -std=c++17 -Wall -Wextra -Wpedantic`: **193 tests / 663 checks, 0
  failures.** This covers all Arduino-independent
  logic (transport incl. keep-alive/retry, parsers, providers incl. vision
  serialization, nested schemas, tools, agent loop, MCP protocol incl. blob
  resources, EdgeStore incl. hooks/TTL, usage metering, provisioning).
- ⏳ **Board compilation** — the library and examples compile for the board
  matrix (ESP32/-S3/-C3 / ESP8266 / Pico W / Uno R4 / Nano 33 IoT / Portenta)
  via **GitHub Actions CI**; ESP32 and Pico W additionally compile-verified
  locally with PlatformIO.
- ⏳ **Hardware-in-the-loop** — running on a physical board (real WiFi, real API
  calls, a real MCP host connection) requires your hardware and **has not been
  executed**. Steps are in `RUNBOOK.md`.

No check is silently treated as passing; the three tiers above are stated
explicitly wherever results are claimed.

## Feature checklist (from the approved Blueprint)

| Feature | Status | Verified by |
|---|---|---|
| Secure HTTP transport (TLS, chunked, SSE), bounded memory | ✅ | native tests |
| HTTP keep-alive + stale-connection replay (v0.7) | ✅ | native tests |
| LLM client — Anthropic, OpenAI, Gemini, Ollama, OpenAI-compatible | ✅ | native tests |
| Structured output (schema-validated JSON, native per provider) | ✅ | native tests |
| Nested schemas — objects, arrays, arrays-of-objects (v0.7) | ✅ | native tests |
| Image/vision input — all 5 providers (v0.7) | ✅ | native tests |
| Transient-failure retry with backoff + Retry-After (v0.7) | ✅ | native tests |
| UsageMeter budgets + ClientMetrics (v0.7) | ✅ | native tests |
| Conversation history (optional, budgeted) | ✅ | native tests |
| Tool calling / agent loop — all 5 providers | ✅ | native tests |
| Shared ToolRegistry (fluent, schema, validation, nested params) | ✅ | native tests |
| MCP server — full method surface, JSON-RPC 2.0 | ✅ | native tests |
| MCP blob (binary) resources (v0.7) | ✅ | native tests |
| MCP Streamable-HTTP transport (request/response leg) | ✅ (compile) | CI |
| mDNS `_mcp._tcp` advertisement — ESP32/ESP8266 (v0.7) | ✅ (compile) | CI |
| EdgeStore KV — RAM + NVS persistence, MCP-exposed, hooks + TTL | ✅ | native tests |
| Deny-by-default mutating tools + bearer auth | ✅ | native tests |
| SSRF guard for URL-fetching tools | ✅ | native tests |
| Secrets in NVS, redacted logging | ✅ | native tests (logic) |
| Provisioning (Serial, declared-fields only) | ✅ | native tests |
| Capability tiering across boards (incl. RP2040, v0.7) | ✅ | native tests (logic) |

## Files audit

- ✅ `README.md` — setup, install, usage for both features, per-board status.
- ✅ `.env.example` — every secret documented.
- ✅ `SECURITY.md`, `RUNBOOK.md` present and current.
- ✅ `library.properties` + `library.json` (Arduino IDE + PlatformIO), `keywords.txt`.
- ✅ CI: native tests, clang-format, cppcheck, gitleaks, board compile matrix.
- ✅ Seed/persistence: EdgeStore persistence round-trip tested; provisioning seeds secrets.
- ✅ No TODO/FIXME/placeholder/commented-out code in shipped source.

## Code quality

- ✅ No exceptions; every fallible API returns `Status`/`Result`.
- ✅ No `std::to_string` in library code (newlib-nano portability); examples use Arduino `String`.
- ✅ Arduino-independent logic isolated from device glue behind interfaces + platform guards.
- ✅ Lint/type rigor: native build is warning-clean under `-Wall -Wextra -Wpedantic`;
  clang-format + cppcheck enforced in CI.

## Security (verified against the threat model)

- ✅ TLS verification on by default; no baked-in certs (generator script); NTP-validated.
- ✅ Secrets in NVS; logger redacts registered secrets; provisioning never echoes values.
- ✅ MCP: deny-by-default mutating tools; optional bearer auth with **constant-time** compare.
- ✅ SSRF guard blocks loopback/private/link-local/CGNAT/metadata.
- ✅ Bounded buffers (HTTP body caps, MCP request caps, agent max-iterations, EdgeStore caps).
- ⚠️ Open items below.

## Known limitations (honest list)

1. **Hardware not yet validated.** Native logic + CI compiles are green; no
   physical-board run has been performed by the author. Run the examples per
   `RUNBOOK.md` before relying on it in production. (v0.7 adds vision,
   keep-alive, retries and mDNS — all covered by native tests / compile checks,
   none yet exercised on a physical board.)
2. **TLS CA pinning covers ESP32, ESP8266 and Pico W.** On WiFiNINA / WiFiS3 /
   mbed boards, trust uses the WiFi co-processor's firmware store;
   per-connection pinning is not yet wired (`setCACert()` is a no-op there).
3. **Persistent secret/KV store coverage.** ESP32 (NVS) and Uno R4 / SAMD
   (`EepromSecretStore`) persist; **ESP8266, Pico W and Portenta** still use
   the in-RAM store — provide an `ISecretStore` backend (or extend
   `EepromSecretStore`) to persist there.
4. **MCP transport is request/response only.** Server-initiated SSE streaming
   (GET stream / notifications, resources/subscribe, list-changed, progress)
   returns 405 (spec-permitted); not implemented. The EdgeStore change hook is
   the designed feed for future `notifications/resources/updated`.
5. **No captive-portal provisioning.** Provisioning is Serial-based; a portal
   frontend can wrap the same `ProvisioningService`.
6. **mDNS advertisement is ESP32/ESP8266 only** (`advertise()` returns false
   elsewhere); Pico W's LEAmDNS port and NINA boards need their own glue.
7. **Vision input is memory-bound.** The base64 frame plus the serialized
   request peak at ~3x the image size in heap; QVGA-class JPEGs are the
   practical ceiling without PSRAM.

## Recommended next phase (ranked by impact)

1. **Hardware validation pass** on an ESP32 (structured generate, vision with an
   ESP32-CAM, agent loop, MCP Inspector connection incl. mDNS discovery) —
   closes the one open verification tier.
2. MCP server-push SSE: GET stream, resources/subscribe (feed it from the new
   EdgeStore onChange hook), notifications/*/list_changed, progress tokens.
3. Cross-board TLS pinning + persistent backends (Pico W flash KV, ESP8266
   LittleFS, NINA / Portenta).
4. Async `generateAsync()` on FreeRTOS/mbed boards (Capabilities already
   advertises `supportsFullDuplex`).
5. Captive-portal provisioning frontend; outbound MCP client for the agent loop.

## Open security questions for the next scale milestone

- Multi-client MCP sessions: current session id is a simple counter (fine for a
  single trusted host; revisit if exposing to many clients).
- Rate limiting on the MCP endpoint is connection-serial only; add per-source
  throttling if the device faces an untrusted LAN.
- Consider encrypting NVS (ESP32 flash encryption) if the physical device is in
  an untrusted location.

## Handoff — quick reference

```bash
make test                         # full native suite (count printed at the end)
python3 tools/gen_ca_bundle.py    # populate TLS roots (once)
# Board build (CI does this; locally needs PlatformIO):
pio ci --project-conf platformio.ini -e esp32dev --lib="." examples/01_SelfTest
pio ci --project-conf platformio.ini -e pico_w   --lib="." examples/01_SelfTest
```

Examples: `01_SelfTest` (any board) → `02_WiFiHealthCheck` → `03–06` (LLM client
+ agent) → `07_McpServer` (MCP) → `08_Provisioning`. Operational procedures live
in `RUNBOOK.md`; the security model in `SECURITY.md`.
