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

## MCP server trust model (read before exposing it)

The MCP server is designed for a **trusted LAN**:

- **Traffic is plaintext HTTP.** A TLS *server* on an MCU is impractical, so the
  bearer token and all data cross the network in the clear. Run the endpoint on a
  trusted LAN/VLAN, or behind a reverse proxy that terminates TLS. **Never expose
  it directly to the internet.**
- **Authentication is opt-in.** With no `setAuthToken()`, the endpoint is open to
  anyone who can route to the device. Always set a long, random
  `MCP_BEARER_TOKEN` for anything beyond a closed test bench. Failed-auth
  responses are delayed (default 500 ms, `setAuthFailDelayMs`) to throttle
  brute-force, and the token is compared in constant time.
- **Writes are deny-by-default** — mutating tools and `kv_set`/`kv_delete` stay
  hidden and refused until explicitly enabled.

## Honest limitations

- A determined attacker on your LAN can still attempt to exhaust a small MCU's
  resources; caps, timeouts and the auth delay reduce but do not eliminate this.
- On WiFiNINA / WiFiS3 boards, certificate trust currently relies on the WiFi
  co-processor firmware store; per-connection pinning is a post-1.0 item. ESP32
  is the fully-validated TLS target today.

## Hardening notes (from the v0.5.0 security review)

- HTTP request serialization strips CR/LF from the request line and all header
  names/values, so a value can never inject extra headers or smuggle a request.
- Bearer-token comparison is constant-time; failed auth is rate-delayed.
- See `COMPLETION.md` for the full review (0 critical / 0 high; residual risk is
  the LAN-trust model above).

## Secrets hygiene

- `.env`, `arduino_secrets.h` and `secrets.h` are gitignored.
- CI runs **gitleaks** (secret scanning) and **cppcheck** (static analysis) on
  every push.
- Never paste real keys into examples, issues, or logs.
