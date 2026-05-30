# EdgeLLM

**Talk to LLMs from Arduino, and let LLMs talk to your Arduino.**

EdgeLLM is a high-performance, security-first Arduino library with **two
independent features**:

1. **LLM client** — call Claude, ChatGPT, Gemini, Ollama (and any
   OpenAI-compatible endpoint) directly from a microcontroller over WiFi+TLS,
   with streaming responses and optional on-device tool calling.
2. **MCP server** — expose your device's sensors, actuators and data to
   networked LLM hosts via the **Model Context Protocol**, so an LLM can
   discover your functions and **read or write** device data on request.

Use either feature on its own — neither pulls in the other.

> **Status: Phase 3 (Tool Calling) complete.** EdgeLLM can chat with Claude,
> OpenAI, Gemini, Ollama and any OpenAI-compatible endpoint (blocking or
> streaming), **and** run an on-device agent loop where the model calls tools you
> register — read a sensor, drive a pin — and gets the results back. The MCP
> server (Phase 4) comes next. See [Roadmap](#roadmap).

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
| ESP8266                 | ✅       | ✅ (1 conn) | ✅ flash KV        | ❌ coop      |
| Arduino Uno R4 WiFi     | ✅       | ⚠️ firmware store¹ | ⏳ Phase 5    | ❌ coop      |
| Nano 33 IoT / MKR (NINA)| ✅       | ⚠️ firmware store¹ | ⏳ Phase 5    | ❌ coop      |
| Portenta (mbed)         | ⏳ Phase 5 | ⏳        | ⏳                 | ✅ mbed RTOS |

¹ On WiFiNINA / WiFiS3 boards the trust store lives in the WiFi co-processor
firmware; per-connection CA pinning is consolidated in Phase 5. ESP32 is the
fully-validated reference target for Phase 1.

In Phase 1, the persistent secret store is implemented for **ESP32 (NVS)**;
other boards use the in-RAM store (sketch supplies secrets at boot) until their
native flash backends land in Phase 5.

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
 └─ llm/         Provider interface + Anthropic/OpenAI/Gemini/Ollama/OpenAI-compatible,
                 LLMClient (blocking + streaming + agent loop), Conversation, Message
```

Portable logic is Arduino-independent (depends only on the STL, which every
target core ships) so it can be unit-tested on the host. On-device glue is
isolated behind interfaces and guarded by platform macros.

## Talk to an LLM (Feature A)

```cpp
#include <EdgeLLM.h>

edge::AnthropicProvider provider(ANTHROPIC_API_KEY);   // or OpenAI / Gemini / Ollama
edge::ArduinoSecureConnection conn;                    // plain conn for local Ollama

void chat() {
  conn.trust().useDefaultBundle();                     // verified TLS
  edge::LLMClient client(provider, conn);
  client.setClock(edge::edgeArduinoMillis);

  // Blocking:
  auto r = client.chat("What is an Arduino?");
  if (r.isOk()) Serial.println(r.value().text.c_str());

  // Streaming (low RAM — nothing buffers the full reply):
  client.chatStream("Tell me a haiku.",
                    [](const std::string& delta) { Serial.print(delta.c_str()); });
}
```

Swap the provider line to change backend; everything else stays the same. Use a
`Conversation` object for managed multi-turn history, or omit it for stateless
single-shot calls.

## Let the model call your device (Feature A, agent loop)

```cpp
edge::ToolRegistry tools;
tools.addTool("set_led", "Turn the built-in LED on or off")
     .paramEnum("state", "on or off", {"on", "off"}, true)
     .mutating().allowWrite()
     .onCall([](edge::ToolCallArgs& a) {
       digitalWrite(LED_BUILTIN, a.getString("state") == "on" ? HIGH : LOW);
       return edge::ToolResult::ok("done");
     });

edge::LLMClient client(provider, conn);
auto r = client.run("Turn the LED on.", tools);  // model calls set_led, then answers
```

The agent loop runs the tools the model asks for, feeds results back, and loops
to a final answer (bounded by `agentOptions().maxIterations`). Tool calling is
implemented for **OpenAI and Anthropic** today; Gemini/Ollama tool calling is the
next increment.

## Roadmap

- **Phase 1 — Foundation & Transport** ✅
- **Phase 2 — LLM client** ✅ — Anthropic, OpenAI, Gemini, Ollama,
  OpenAI-compatible; blocking + streaming; conversation history.
- **Phase 3 — Tool calling** ✅ *(this release)* — shared `ToolRegistry`,
  on-device agent loop (OpenAI + Anthropic), SSRF guard.
- **Phase 4 — MCP server:** full JSON-RPC 2.0 / Streamable HTTP, tools,
  resources, prompts, built-in `EdgeStore` KV, deny-by-default writes, auth.
- **Phase 5 — Cross-board + provisioning + hardening:** Uno R4 / NINA / Portenta
  bring-up, provisioning portal, security pass, release.

## Security

See [SECURITY.md](SECURITY.md) for the security model and reporting policy, and
[RUNBOOK.md](RUNBOOK.md) for operational procedures (secret rotation, CA
refresh, troubleshooting).

## License

MIT — see [LICENSE](LICENSE).
