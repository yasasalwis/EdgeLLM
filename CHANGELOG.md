# Changelog

All notable changes to EdgeLLM are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow SemVer.

## [0.7.0] — Vision, nested schemas, keep-alive, retries, budgets & Pico W

### Added — LLM client
- **Image input (vision)** for **all five providers**: attach a base64 image to
  a user message (`Message::userWithImage(text, b64, mime)`) or call
  `client.generate(schema, system, user, imageBase64, mime)`. Serialized in
  each provider's native multimodal format (Anthropic content blocks, OpenAI
  data-URI `image_url`, Gemini `inline_data`, Ollama `images`) — an ESP32-CAM
  frame can go straight to a vision model and come back as validated JSON.
- **Nested schemas**: `FieldSpec` describes object shapes; `ResponseSchema` and
  `ToolBuilder` gain `objectField(...)` / `arrayField(...)` (arrays of
  primitives or of objects) / `paramObject(...)` / `paramArray(...)`. The JSON
  Schema writer emits nested `properties`/`items`, and validation recurses
  through nested objects and array elements. "Return a list of items with
  these fields" — the most common extraction shape — now works end-to-end.
- **Transient-failure retries**: 429/500/502/503/504 and transient transport
  errors (connect/DNS/TLS/timeout/dropped socket) are retried with exponential
  backoff + jitter, honoring a numeric `Retry-After`. Tune or disable via
  `client.retryPolicy()`; wire `setDelayFn(edge::edgeArduinoDelay)` so waits
  yield to the scheduler. Certificate failures, auth errors and 400s are never
  retried.
- **UsageMeter** — spend guard for always-on devices: request and total-token
  caps; when exhausted, calls fail fast with `Error::BudgetExceeded` (new
  error code) until `reset()`.
- **ClientMetrics** — `client.metrics()`: requests, retries, transport/HTTP
  errors, schema retries, token totals, last/total latency.
- **ChatOptions**: `topP`, `stopSequences` (serialized natively by all four
  wire formats) and `extraHeaders` (e.g. OpenRouter attribution) applied to
  every request.

### Added — transport
- **HTTP keep-alive**: `LLMClient` now reuses one TLS connection across all the
  requests inside a `generate()`/`run()` call — every agent-loop round and
  validation retry previously paid a full TCP+TLS handshake (seconds + a heap
  spike on ESP32). A stale reused socket (server closed it idle) is detected
  and replayed once on a fresh connection per RFC 7230 §6.3.1.
  `setPersistentConnection(true)` keeps it open between calls too.
- `HttpClient::setKeepAlive` / `connectionReusable`; `serializeRequest` gains a
  keep-alive default-header variant; the Content-Length body reader can no
  longer consume bytes belonging to the next response on a reused connection.

### Added — MCP server
- **Blob resources**: `ResourceBuilder::blob()` marks a resource binary — its
  handler returns base64 and `resources/read` serves it in the spec's `blob`
  field (e.g. serve a camera frame to an MCP host).
- **mDNS discovery**: `McpHttpServer::advertise(hostname)` announces
  `<hostname>.local` with a `_mcp._tcp` service and the endpoint path in TXT
  (ESP32 / ESP8266; returns false elsewhere). No more hunting for the IP in
  the serial monitor.
- **EdgeStore**: `setOnChange(...)` change hook (react when a host writes a
  key) and `setWithTtl(...)` expiring entries (lazy eviction, RAM-only, never
  persisted; a TTL overwrite of a persisted key also drops the stale flash
  copy).

### Added — boards
- **Raspberry Pi Pico W (RP2040)** support: platform detection, BearSSL TLS
  connection (the arduino-pico core mirrors the ESP8266 API), NTP time, and a
  `pico_w` PlatformIO env in the CI matrix. Persistent secrets on RP2040 are
  RAM-only for now (like ESP8266).
- **ESP32-S3 and ESP32-C3** compile-verified via new `esp32s3` / `esp32c3`
  envs in the CI matrix.

### Fixed
- **OpenAI strict mode with optional fields**: `strict:true` was always sent,
  but OpenAI rejects strict schemas whose fields aren't all required. The
  provider now downgrades to non-strict automatically (local validation still
  applies) — schemas with optional fields no longer 400.
- Ollama tool requests now carry the sampling options (`num_predict`,
  `temperature`, ...) that the structured path already sent.

## [0.6.1] — Cross-board: Portenta + persistent store for Uno R4 / SAMD

### Added
- **Portenta H7 support** — new `portenta_h7` PlatformIO env + CI matrix entry.
  The full library and a networking example (WiFi + TLS + NTP + structured
  generate) compile and link for the mbed core.
- **`EepromSecretStore`** — persistent secret/KV store for non-ESP32 boards, over
  the Arduino EEPROM API: **Uno R4 WiFi** (built-in EEPROM) and **SAMD / Nano 33
  IoT** (via the `FlashStorage` library's emulated EEPROM). Usable directly or as
  an `EdgeStore` persistence backend.
- **`EepromCodec`** — pure, host-unit-tested serializer for the store's byte
  layout (magic/version/length-prefixed entries; truncation- and corruption-safe).
- Example `08_Provisioning` now selects the persistent backend per board (NVS on
  ESP32, EEPROM/flash on Uno R4 / SAMD, RAM elsewhere).

### Changed
- Per-board status table corrected: Uno R4 / SAMD now have persistent secrets;
  ESP8266 persistent storage is honestly marked RAM-only (it is not ESP32, so it
  had no NVS backend — the earlier "flash KV" label was inaccurate).

### Verified
- All five board families (ESP32, ESP8266, Uno R4, SAMD/Nano 33 IoT, Portenta)
  **compile-verified** locally with PlatformIO; +6 native tests (EEPROM codec)
  for 145 total.

### Still deferred (post-1.0)
- Per-connection TLS CA pinning on WiFiNINA / WiFiS3 / mbed (co-processor firmware
  trust model); ESP8266 / Portenta persistent backends; on-hardware runtime
  validation.

## [0.6.0] — Structured output only (BREAKING)

The LLM client now returns **structured output exclusively**: you supply system +
user messages and a `ResponseSchema`, and the model returns a JSON object that is
validated against it. Free-text chat and text streaming are removed.

### Added
- **`ResponseSchema`** — fluent schema builder (reuses the tool field model).
- **`StructuredResult`** — typed accessor over the validated JSON (`getString`,
  `getInt`, `getNumber`, `getBool`, `json()`).
- **`LLMClient::generate(schema, system, user)`** (and message-list / Conversation
  overloads) — schema-constrained generation with local validation and an
  automatic retry on validation failure (`setStructuredRetries`).
- **Native structured-output enforcement per provider:** OpenAI `response_format`
  json_schema, Anthropic forced single-tool, Gemini `responseSchema`, Ollama
  `format` — then validated locally against the same schema.
- **`writeObjectSchema` / `validateObject`** shared schema util (now backs both
  tool input schemas and response schemas).
- New examples: `04_DataExtraction` (replaces the streaming example), and
  structured versions of `03`, `05`, `06`.

### Changed (breaking)
- Removed `LLMClient::chat()` and `chatStream()`; use `generate()`.
- `LLMClient::run(...)` now takes a `ResponseSchema` first and returns a
  `Result<StructuredResult>`; the agent loop runs tools, then produces a final
  schema-validated answer.
- `Provider` interface: `buildChatRequest`/`parseChatResponse`/`parseStreamEvent`
  replaced by `buildStructuredRequest`/`parseStructuredResponse`. Tool methods
  unchanged. `StreamDelta` and `StreamFormat` removed.
- `ChatResult` is now response metadata only (no `text`).

### Notes
- `HttpClient::sendStream` and `SseParser` remain as general transport utilities
  (still tested) but are no longer used by the LLM client.

## [0.5.1] — Security review fixes & edge-case hardening

### Security
- **HTTP header-injection guard** — request serialization strips CR/LF from the
  method, path, host, and every header name/value (CWE-93/113 defense in depth).
- **MCP bearer-token brute-force throttle** — configurable delay before a 401
  (`setAuthFailDelayMs`, default 500 ms), on top of the constant-time compare.

### Robustness / edge cases
- **Unsupported-board guard** — a clear compile-time `#error` (instead of cryptic
  STL errors) when built for a core without the C++ STL, e.g. classic AVR Arduino
  (Uno/Mega) + WiFi shield, naming the supported 32-bit WiFi cores.
- **No duplicate registration** — re-registering a tool / resource / prompt with
  an existing name now redefines it in place instead of creating a duplicate
  (duplicates would confuse the model and violate MCP's unique-name requirement).
- **EdgeStore reserved key** — the internal persistence-manifest key can no longer
  be overwritten or deleted by a user/host, preventing store corruption.
- `EdgeStore`'s manifest key is now file-local to avoid static-member linkage
  issues on `-std=gnu++11` Arduino cores.

### Tests
- +5 native tests (header-injection ×2, registry dedupe ×3, reserved-key) for 141
  total, all green.

## [0.5.0] — Phase 5: Parity, Provisioning & Hardening

### Added
- **Tool calling for Gemini and Ollama** — completing tool/function-calling
  across all five providers (Gemini `functionCall`/`functionResponse`, Ollama
  `tool_calls` with object arguments). The agent loop now works with any of them.
- **Provisioning** (optional, off by default): `ProvisioningService` (pure,
  declares allowed fields, never echoes secret values) + `SerialProvisioner`
  (device frontend) to set WiFi creds and API keys into NVS without recompiling.
- **Example `08_Provisioning`** — provision secrets over the Serial Monitor.
- **Tests:** +14 native tests (Gemini/Ollama tool calling, provisioning) for 136
  total.

### Security
- **Constant-time bearer-token comparison** in the MCP server (the previous `==`
  short-circuited and leaked timing).
- Provisioning writes are restricted to explicitly declared fields and never
  echo submitted secret values.

### Deferred to a future release (documented in COMPLETION.md)
- Hardware validation on non-ESP32 boards; per-connection CA pinning and
  persistent secret/KV backends for Uno R4 / NINA / Portenta.
- Captive-portal provisioning frontend; MCP server-initiated SSE stream.

## [0.4.0] — Phase 4: MCP Server

### Added
- **`McpServer`** — Model Context Protocol server core (JSON-RPC 2.0, pure
  string-in/string-out so it's fully host-testable). Implements `initialize`,
  `ping`, `notifications/*`, `tools/list`, `tools/call`, `resources/list`,
  `resources/templates/list`, `resources/read`, `prompts/list`, `prompts/get`,
  with batch support, JSON-RPC error codes, capability negotiation, protocol
  version echo, and `Mcp-Session-Id` sessions.
- **`McpHttpServer`** — on-device Streamable-HTTP transport over the portable
  `WiFiServer` (POST → JSON response; 405 on GET per spec; size/timeout caps).
- **`EdgeStore`** — built-in RAM KV datastore with bounded size and optional NVS
  persistence (manifest-based), auto-exposed over MCP as `kv://<key>` resources
  (read) and `kv_set` / `kv_delete` tools (write).
- **`ResourceRegistry`** and **`PromptRegistry`** — fluent, handler-based
  resources and prompts.
- **Shared registry:** the MCP server consumes the same Phase 3 `ToolRegistry`,
  so one tool serves both the agent loop and networked hosts.
- **Example `07_McpServer`** — ESP32 exposes get_temp/set_led tools, a device
  status resource, and the persistent KV store; connect with MCP Inspector or
  Claude Desktop at `http://<ip>:8080/mcp`.
- **Tests:** +27 native tests (EdgeStore caps/persistence, full protocol surface,
  resources/prompts) for 122 total.

### Security
- **Mutating tools are deny-by-default**: excluded from `tools/list` and refused
  by `tools/call` unless `allowWrite()` is set; `kv_set`/`kv_delete` are off
  unless `setStore(store, allowWrites=true)`. Tools are annotated with
  `readOnlyHint`/`destructiveHint`.
- **Optional bearer-token auth** gates the whole endpoint (401 on mismatch).
- Bounded request body, header-line, and read-timeout caps in the HTTP glue.

## [0.3.0] — Phase 3: Tool Calling & Agent Loop

### Added
- **Shared `ToolRegistry`** with a fluent builder
  (`addTool(...).param(...).paramEnum(...).mutating().allowWrite().onCall(...)`),
  JSON-Schema generation, argument validation (required/type/enum), and dispatch.
  This is the registry the MCP server (Phase 4) will also consume.
- **`ToolCallArgs`** typed accessor over a call's JSON arguments.
- **Function calling** for **OpenAI** and **Anthropic**: request building
  (advertising tools + serializing assistant tool-call turns and tool results,
  including Anthropic's coalesced tool_result blocks) and response parsing.
- **Agent loop** in `LLMClient::run()` — executes requested tools on-device,
  feeds results back, and loops to a final answer, with a `maxIterations` guard
  and graceful handling of unknown-tool / bad-argument cases.
- **`UrlGuard`** SSRF helper (`hostFromUrl`, `isBlockedHost`, `isUrlAllowed`) for
  tools that fetch user-supplied URLs — blocks loopback/private/link-local/CGNAT
  and cloud-metadata addresses.
- **Example `06_AgentTools`** — Claude calls on-device `get_uptime` (read) and
  `set_led` (mutating) tools.
- **Tests:** +27 native tests (registry/schema/validation, provider tool
  request/response, end-to-end agent loop, SSRF guard) for 95 total.

### Notes
- Tool calling is implemented for OpenAI and Anthropic this phase; Gemini and
  Ollama tool calling return `NotImplemented` from the agent loop (their plain
  chat is unaffected) and are the next increment.
- Mutating/allowWrite annotations are carried now but enforced by the MCP server
  in Phase 4; the agent loop runs any registered tool (registering is opt-in).

## [0.2.0] — Phase 2: LLM Client

### Added
- **Provider abstraction** (`Provider`) with five implementations: Anthropic
  (Claude), OpenAI, Google Gemini, Ollama, and a configurable
  `OpenAIChatProvider` that also serves as the generic OpenAI-compatible adapter
  (Groq, OpenRouter, LM Studio, llama.cpp, vLLM, ...). Pluggable: implement the
  interface to add your own backend.
- **`LLMClient`** — blocking `chat()` and token-by-token `chatStream()` over the
  Phase 1 transport, with HTTP status → error mapping and bounded streaming.
- **Streaming** for both SSE providers and Ollama's NDJSON format.
- **`Conversation`** — optional managed multi-turn history with a byte budget and
  oldest-turn trimming; stateless single-shot still supported (hybrid model).
- **Examples:** `03_CloudChat` (Claude, blocking), `04_StreamingChat` (OpenAI,
  streaming), `05_OllamaLocal` (local, plain HTTP, no key).
- **Transport:** `HttpClient::sendStream()` delivers the body incrementally
  (chunked-decoded) to a callback, keeping RAM bounded for long replies.
- **Tests:** +23 native tests (providers, conversation, end-to-end client) for
  68 total; ArduinoJson 7.4.3 vendored for the host test build only.

### Notes
- Default model ids are best-effort current values; override via
  `ChatOptions.model` or the provider constructor.
- Gemini passes the API key in the URL (provider design); the client registers
  it with the logger for redaction and avoids logging full request paths.

## [0.1.0] — Phase 1: Foundation & Transport

### Added
- **Core (Arduino-independent):** `Version`, `Error` taxonomy with safe
  descriptions, exception-free `Status`/`Result`, redacting `Logger` with a sink
  interface, and per-board `Capabilities` detection/tiering.
- **Transport:** abstract `IConnection`; `HttpClient` (blocking HTTP/1.1 over any
  connection) with Content-Length and chunked body framing, size caps and an
  injectable clock for timeouts; pure `HttpRequestBuilder`, incremental
  `SseParser` and `ChunkedDecoder`; `CACertStore` trust policy.
- **HAL:** `ISecretStore` with in-memory and ESP32 NVS (`Preferences`)
  implementations; `ITimeSource` with NTP backend; `ArduinoSecureConnection` /
  `ArduinoPlainConnection`; Serial log sink; `millis` clock adapter; platform
  detection.
- **Examples:** `01_SelfTest` (any board) and `02_WiFiHealthCheck` (ESP32).
- **Tooling:** native unit-test suite (45 tests) via Makefile and CMake;
  PlatformIO board matrix; GitHub Actions CI (native tests, clang-format,
  cppcheck, gitleaks, compile matrix); `tools/gen_ca_bundle.py`.
- **Docs:** README, SECURITY, RUNBOOK, `.env.example`, library manifests for
  Arduino IDE and PlatformIO.

### Security
- TLS verification on by default; no baked-in certificates; NTP-validated
  certificate windows; secrets in NVS and redacted from logs; bounded response
  buffers.

### Notes
- ESP32 is the fully-validated reference board this phase. ESP8266 compiles with
  a 1-connection TLS tier. Uno R4 / Nano 33 IoT compile; per-connection CA
  pinning and persistent secret backends arrive in Phase 5.
