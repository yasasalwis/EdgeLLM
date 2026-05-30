# EdgeLLM — Completion Audit & Handoff

This document is the honest, end-of-build audit for EdgeLLM v0.5.0. It records
what is done, how it was verified, and what remains — so the next engineer can
pick this up cold.

## Verification environment (what "verified" means here)

- ✅ **Native unit tests** — compiled and **executed** on the host with
  `clang++ -std=c++17 -Wall -Wextra -Wpedantic`: **136 tests / 441 checks, 0
  failures.** This covers all Arduino-independent logic (transport, parsers,
  providers, tools, agent loop, MCP protocol, EdgeStore, provisioning).
- ⏳ **Board compilation** — the library and all 8 examples compile for the board
  matrix (ESP32 / ESP8266 / Uno R4 / Nano 33 IoT) via **GitHub Actions CI**.
  PlatformIO is not installed in the build environment used to author this, so
  board builds are CI-verified, not locally verified.
- ⏳ **Hardware-in-the-loop** — running on a physical board (real WiFi, real API
  calls, a real MCP host connection) requires your hardware and **has not been
  executed**. Steps are in `RUNBOOK.md`.

No check is silently treated as passing; the three tiers above are stated
explicitly wherever results are claimed.

## Feature checklist (from the approved Blueprint)

| Feature | Status | Verified by |
|---|---|---|
| Secure HTTP transport (TLS, chunked, SSE), bounded memory | ✅ | native tests |
| LLM client — Anthropic, OpenAI, Gemini, Ollama, OpenAI-compatible | ✅ | native tests |
| Blocking + streaming chat (SSE + NDJSON) | ✅ | native tests |
| Conversation history (optional, budgeted) | ✅ | native tests |
| Tool calling / agent loop — all 5 providers | ✅ | native tests |
| Shared ToolRegistry (fluent, schema, validation) | ✅ | native tests |
| MCP server — full method surface, JSON-RPC 2.0 | ✅ | native tests |
| MCP Streamable-HTTP transport (request/response leg) | ✅ (compile) | CI |
| EdgeStore KV — RAM + NVS persistence, MCP-exposed | ✅ | native tests |
| Deny-by-default mutating tools + bearer auth | ✅ | native tests |
| SSRF guard for URL-fetching tools | ✅ | native tests |
| Secrets in NVS, redacted logging | ✅ | native tests (logic) |
| Provisioning (Serial, declared-fields only) | ✅ | native tests |
| Capability tiering across boards | ✅ | native tests (logic) |

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
   `RUNBOOK.md` before relying on it in production.
2. **TLS CA pinning is ESP32-complete.** On WiFiNINA / WiFiS3 boards, trust uses
   the WiFi co-processor's firmware store; per-connection pinning is not yet wired.
3. **Persistent secret/KV store is ESP32 (NVS) only.** Other boards use the
   in-RAM store; provide an `ISecretStore` backend to persist there.
4. **MCP transport is request/response only.** Server-initiated SSE streaming
   (GET stream / notifications) returns 405 (spec-permitted); not implemented.
5. **No captive-portal provisioning.** Provisioning is Serial-based; a portal
   frontend can wrap the same `ProvisioningService`.

## Recommended next phase (ranked by impact)

1. **Hardware validation pass** on an ESP32 (chat, streaming, agent loop, MCP
   Inspector connection) — closes the one open verification tier.
2. Cross-board TLS pinning + persistent backends (Uno R4 / NINA / Portenta).
3. MCP server-push SSE for long-running tools / notifications.
4. Captive-portal provisioning frontend.
5. Optional: response-time metrics, OTA secret rotation helpers.

## Open security questions for the next scale milestone

- Multi-client MCP sessions: current session id is a simple counter (fine for a
  single trusted host; revisit if exposing to many clients).
- Rate limiting on the MCP endpoint is connection-serial only; add per-source
  throttling if the device faces an untrusted LAN.
- Consider encrypting NVS (ESP32 flash encryption) if the physical device is in
  an untrusted location.

## Handoff — quick reference

```bash
make test                         # 136 native tests
python3 tools/gen_ca_bundle.py    # populate TLS roots (once)
# Board build (CI does this; locally needs PlatformIO):
pio ci --project-conf platformio.ini -e esp32dev --lib="." examples/01_SelfTest
```

Examples: `01_SelfTest` (any board) → `02_WiFiHealthCheck` → `03–06` (LLM client
+ agent) → `07_McpServer` (MCP) → `08_Provisioning`. Operational procedures live
in `RUNBOOK.md`; the security model in `SECURITY.md`.
