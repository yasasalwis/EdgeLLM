# Security Policy

EdgeLLM treats security as a design constraint, verified at every phase — not a
final checklist. This document summarizes the model and how to report issues.

## Reporting a vulnerability

Please report security issues privately to **yasaslive@gmail.com** with:

- A description of the issue and its impact.
- Steps to reproduce (a minimal sketch is ideal).
- Affected board/core and EdgeLLM version.

Do **not** open a public issue for security reports. We aim to acknowledge
within 5 business days. Coordinated disclosure is appreciated; we will credit
reporters who wish to be named.

## Security model

### Assets protected
- LLM provider API keys, WiFi credentials, MCP bearer tokens.
- The device's actuator/data surface exposed via the (upcoming) MCP server.
- Conversation content in transit and in RAM.

### Defaults (Phase 1)
- **TLS verification is ON.** The transport will not silently fall back to
  insecure. `setInsecure()` is an explicit, logged opt-in intended only for a
  trusted local endpoint (e.g. Ollama on the LAN).
- **No baked-in certificates.** Roots are generated from the current Mozilla set
  via `tools/gen_ca_bundle.py`, avoiding stale-trust pitfalls.
- **Time-validated TLS.** NTP sync is provided so certificate validity windows
  are actually checked; without valid time, verification fails closed.
- **Secrets in NVS, never in source by default.** On ESP32 secrets live in NVS
  (`Preferences`). The in-RAM store is the fallback for boards without a flash
  backend yet.
- **Logs are scrubbed.** The logger redacts any registered secret value and
  provides `maskToken()`; sensitive values are never logged in cleartext.
- **No exceptions / fail-as-value.** Every fallible API returns a `Status` /
  `Result`, so failures can't be silently swallowed.
- **Bounded memory.** Response bodies are size-capped; SSE and chunked decoding
  are incremental to resist memory-exhaustion.

### Coming with later phases (already designed in the threat model)
- **MCP writes are deny-by-default.** Mutating tools are disabled until
  explicitly enabled, are annotated (`readOnly`/`destructive` hints), validated
  against a JSON Schema before dispatch, and can require a bearer token.
- **MCP server hardening:** LAN binding, connection/size/timeout caps,
  per-tool authorization callbacks.
- **Agent loop guardrails:** max-iteration limit and SSRF checks on
  user-supplied URLs.

## Honest limitations

- A determined attacker on your LAN can still attempt to exhaust a small MCU's
  resources; caps and auth reduce but do not eliminate this.
- On WiFiNINA / WiFiS3 boards, certificate trust currently relies on the WiFi
  co-processor firmware store; per-connection pinning arrives in Phase 5. ESP32
  is the fully-validated TLS target today.

## Secrets hygiene

- `.env`, `arduino_secrets.h` and `secrets.h` are gitignored.
- CI runs **gitleaks** (secret scanning) and **cppcheck** (static analysis) on
  every push.
- Never paste real keys into examples, issues, or logs.
