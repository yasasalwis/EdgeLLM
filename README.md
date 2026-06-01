# EdgeLLM

**Talk to LLMs from Arduino, and let LLMs talk to your Arduino.**

EdgeLLM is a high-performance, security-first Arduino library with **two
independent features**:

1. **LLM client** — call Claude, ChatGPT, Gemini, Ollama (and any
   OpenAI-compatible endpoint) directly from a microcontroller over WiFi+TLS, and
   get back **structured, schema-validated JSON** (with optional on-device tool
   calling). Structured output is the only response mode.
2. **MCP server** — expose your device's sensors, actuators and data to
   networked LLM hosts via the **Model Context Protocol**, so an LLM can
   discover your functions and **read or write** device data on request.

Use either feature on its own — neither pulls in the other.

> **Status: feature-complete (v0.6.0).** Both features are done: the **LLM
> client** returns schema-validated **structured output** with an on-device tool
> calling agent loop across **all five** providers, and the **MCP server**
> (tools, resources, prompts, built-in KV store, deny-by-default writes, bearer
> auth). 139 native tests pass. Remaining work is **hardware validation** and a
> few cross-board items — see [COMPLETION.md](COMPLETION.md) for the honest
> audit. See [Roadmap](#roadmap).

---

## Why a foundation-first release?

Talking to cloud LLMs from an 80 KB microcontroller is mostly a *transport and
memory* problem, not an API problem. EdgeLLM is built bottom-up so every higher
feature stands on verified ground:

- **Bounded memory.** SSE and chunked-transfer parsing are incremental — the
  device never buffers a whole response. Response bodies are size-capped.
- **Secure by default.** TLS verification is on; secrets live in NVS and are
  scrubbed from logs; writes (when the MCP server arrives) are deny-by-default.
- **Capability-tiered.** One API, one HAL, graceful degradation from ESP32 down
  to ESP8266 — the library knows what each board can do.
- **Actually tested.** The portable logic (HTTP, SSE, chunked decoding, secret
  store, redaction) has a native unit-test suite that runs on every commit.

## Supported boards (per-board status)

| Board family            | Compiles | TLS client | Persistent secrets | Full-duplex |
|-------------------------|:--------:|:----------:|:------------------:|:-----------:|
| **ESP32** (reference)   | ✅       | ✅ verified | ✅ NVS             | ✅ FreeRTOS |
| ESP8266                 | ✅       | ✅ (1 conn) | ⏳ (RAM)           | ❌ coop      |
| Arduino Uno R4 WiFi     | ✅       | ⚠️ firmware store¹ | ✅ EEPROM     | ❌ coop      |
| Nano 33 IoT / MKR (NINA)| ✅       | ⚠️ firmware store¹ | ✅ flash²     | ❌ coop      |
| Portenta (mbed)         | ✅       | ⚠️ firmware store¹ | ⏳ (RAM)      | ✅ mbed RTOS |

All five compile in CI. ¹ On WiFiNINA / WiFiS3 / mbed boards the trust store
lives in the WiFi co-processor firmware; per-connection CA pinning is a post-1.0
item — ESP32 is the fully-validated TLS target. ² SAMD persistence uses the
`FlashStorage` library (emulated EEPROM).

> **Verification note:** every board family above is **compile-verified** (the
> full library + an example build cleanly, including the gnu++11 SAMD toolchain).
> On-hardware runtime validation is still pending — see [COMPLETION.md](COMPLETION.md).

> **Not supported: classic AVR Arduino (Uno / Mega) + a WiFi shield.** Those chips
> have no C++ STL and only a few KB of RAM — they cannot do TLS to a cloud LLM and
> the library won't compile. The Library Manager marks EdgeLLM incompatible with
> AVR, and an explicit `#error` explains why if you force it. Use a 32-bit WiFi
> core (the table above). "Arduino + WiFi" that *is* supported means the Uno R4
> WiFi or a MKR/Nano 33 IoT with WiFiNINA.

Persistent secret storage: **ESP32** (NVS, `PreferencesSecretStore`), **Uno R4 /
SAMD** (`EepromSecretStore`). ESP8266 and Portenta currently use the in-RAM store
(supply secrets at boot, e.g. via Serial provisioning) until their flash backends
land.

## Install

### PlatformIO

```ini
# platformio.ini
lib_deps =
    https://github.com/edgellm/EdgeLLM.git
    bblanchon/ArduinoJson@^7.0.0
```

### Arduino IDE

Library Manager → search **EdgeLLM** (also install **ArduinoJson**). Or download
this repo as a ZIP and use *Sketch → Include Library → Add .ZIP Library*.

## Quick start — Self Test (any board, no network)

`examples/01_SelfTest` boots the library, prints a capability report, and
demonstrates the secret store + log redaction:

```cpp
#include <EdgeLLM.h>

edge::Logger logger;
edge::SerialLogSink serialSink;

void setup() {
  Serial.begin(115200);
  logger.setSink(&serialSink);
  edge::Capabilities caps = edge::detectCapabilities();
  logger.info(std::string("EdgeLLM on ") + caps.board);
}
void loop() {}
```

Then try `examples/02_WiFiHealthCheck` (ESP32) for a real WiFi + HTTP(S)
round-trip through the transport.

## Secure TLS — generating the CA bundle

EdgeLLM does **not** ship baked-in root certificates (they rotate and go stale).
Generate a current Mozilla bundle once:

```bash
python3 tools/gen_ca_bundle.py
```

This writes `src/transport/RootCABundle.h`. After that, verified HTTPS works
out of the box (`conn.trust().useDefaultBundle()`), or pin a single root with
`conn.trust().setCACert(pem)`. Use `conn.trust().setInsecure()` **only** for a
trusted local endpoint such as Ollama on your LAN.

## Running the tests

```bash
make test          # build + run the native unit suite (needs a C++17 compiler)
# or, with CMake / CLion:
cmake -S . -B build && cmake --build build && ./build/edgellm_tests
```

Board compilation is verified in CI via PlatformIO across the board matrix; see
`.github/workflows/ci.yml`.

## Architecture (foundation)

```
EdgeLLM.h ─ umbrella
 ├─ core/        Version, Errors, Result (no exceptions), Logger (redacting), Capabilities
 ├─ transport/   IConnection, HttpClient (+ streaming), HttpRequestBuilder, SseParser,
 │               ChunkedDecoder, CACertStore
 ├─ hal/         Platform detection, ISecretStore (+ Memory/NVS impls), ITimeSource (+ NTP),
 │               ArduinoConnection (TLS/plain), Serial log sink, millis clock
 ├─ tools/       ToolRegistry (fluent), Tool + JSON schema, ToolCallArgs, UrlGuard (SSRF)
 ├─ llm/         Provider interface + Anthropic/OpenAI/Gemini/Ollama/OpenAI-compatible,
 │               LLMClient (structured generate + agent loop), ResponseSchema,
 │               StructuredResult, Conversation, Message
 ├─ mcp/         McpServer (JSON-RPC core), McpHttpServer (transport), EdgeStore (KV),
 │               ResourceRegistry, PromptRegistry
 └─ provisioning/ ProvisioningService (pure), SerialProvisioner (device)
```

Portable logic is Arduino-independent (depends only on the STL, which every
target core ships) so it can be unit-tested on the host. On-device glue is
isolated behind interfaces and guarded by platform macros.

## Structured output (Feature A — the only response mode)

You always get back a JSON object validated against a schema you define:

```cpp
#include <EdgeLLM.h>

edge::AnthropicProvider provider(ANTHROPIC_API_KEY);   // or OpenAI / Gemini / Ollama
edge::ArduinoSecureConnection conn;                    // plain conn for local Ollama

void ask() {
  conn.trust().useDefaultBundle();                     // verified TLS
  edge::LLMClient client(provider, conn);
  client.setClock(edge::edgeArduinoMillis);

  edge::ResponseSchema schema("city_facts");
  schema.field("name", edge::ParamType::String, "the city")
        .field("population", edge::ParamType::Integer, "approx population")
        .field("coastal", edge::ParamType::Boolean, "is it on the coast?");

  auto r = client.generate(schema, "Return concise facts.", "Tell me about Kyoto.");
  if (r.isOk()) {
    Serial.println(r.value().getString("name").c_str());
    Serial.println(r.value().getInt("population"));
    Serial.println(r.value().getBool("coastal"));
    // r.value().json() is the raw validated JSON
  }
}
```

The library uses each provider's **native** structured-output feature (OpenAI
`response_format`, Anthropic forced tool, Gemini `responseSchema`, Ollama
`format`) and then **validates the result locally**, retrying once on a mismatch.
Swap the provider line to change backend; everything else stays the same.

## Let the model call your device, then answer in structure (agent loop)

```cpp
edge::ToolRegistry tools;
tools.addTool("set_led", "Turn the built-in LED on or off")
     .paramEnum("state", "on or off", {"on", "off"}, true)
     .mutating().allowWrite()
     .onCall([](edge::ToolCallArgs& a) {
       digitalWrite(LED_BUILTIN, a.getString("state") == "on" ? HIGH : LOW);
       return edge::ToolResult::ok("done");
     });

edge::ResponseSchema schema("result");
schema.field("action_taken", edge::ParamType::String, "what you did")
      .field("led_on", edge::ParamType::Boolean, "is the LED on now?");

edge::LLMClient client(provider, conn);
auto r = client.run(schema, "Turn the LED on.", tools);  // runs tools, then returns structured JSON
```

The agent loop runs the tools the model asks for, feeds results back (bounded by
`agentOptions().maxIterations`), then produces a final schema-validated answer.
Works across **all five providers** (Anthropic, OpenAI, Gemini, Ollama, and
OpenAI-compatible).

## Expose your device to LLM hosts (Feature B, MCP server)

```cpp
edge::ToolRegistry tools;        // the SAME registry the agent loop uses
tools.addTool("get_temp", "Read temperature").onCall(/* ... */);

edge::EdgeStore store;           // built-in KV, readable/writable over MCP
edge::McpServer mcp("EdgeLLM-Device", EDGELLM_VERSION);
edge::McpHttpServer http(mcp, 8080, "/mcp");

void setup() {
  mcp.setToolRegistry(&tools);
  mcp.setStore(&store, /*allowWrites=*/true);   // expose kv_set/kv_delete
  // mcp.setAuthToken("a-long-token");           // optional bearer auth
  http.begin();                                  // after WiFi is up
}
void loop() { http.handle(); }                   // point a host at http://<ip>:8080/mcp
```

Connect from the [MCP Inspector](https://github.com/modelcontextprotocol/inspector)
(`npx @modelcontextprotocol/inspector`, Streamable HTTP transport) or any MCP
host. **Writes are deny-by-default**: a mutating tool stays hidden until you call
`.allowWrite()`, and the KV write tools require `allowWrites=true`.

## Roadmap

- **Phase 1 — Foundation & Transport** ✅
- **Phase 2 — LLM client** ✅ — Anthropic, OpenAI, Gemini, Ollama,
  OpenAI-compatible; conversation history.
- **Phase 3 — Tool calling** ✅ — shared `ToolRegistry`, on-device agent loop,
  SSRF guard.
- **Phase 4 — MCP server** ✅ — JSON-RPC 2.0 / Streamable HTTP,
  tools/resources/prompts, built-in `EdgeStore` KV, deny-by-default writes,
  bearer auth.
- **Phase 5 — Parity, provisioning & hardening** ✅ — tool calling for **all
  five** providers, Serial provisioning, constant-time auth.
- **v0.6.0 — Structured output only** ✅ *(this release)* — `generate()` returns
  schema-validated JSON via each provider's native structured-output feature;
  free-text chat and streaming removed.
- **Remaining (post-1.0 candidates):** on-hardware validation, cross-board TLS
  pinning + persistent backends (Uno R4 / NINA / Portenta), captive-portal
  provisioning, MCP server-push SSE. See [COMPLETION.md](COMPLETION.md).

## Security

See [SECURITY.md](SECURITY.md) for the security model and reporting policy, and
[RUNBOOK.md](RUNBOOK.md) for operational procedures (secret rotation, CA
refresh, troubleshooting).

## License

MIT — see [LICENSE](LICENSE).
