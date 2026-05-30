# Changelog

All notable changes to EdgeLLM are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow SemVer.

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
