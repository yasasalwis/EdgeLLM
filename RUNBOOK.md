# EdgeLLM Runbook

Operational procedures for developing, flashing, and maintaining devices that
use EdgeLLM. This grows with each phase; Phase 1 covers the foundation.

## Onboard a new contributor

1. Clone the repo. Install a C++17 compiler (clang or gcc) and, for board
   builds, [PlatformIO](https://platformio.org/) (`pip install platformio`).
2. Run the native tests: `make test`. All tests must pass.
3. (Optional) Open in CLion: it picks up `CMakeLists.txt` and builds
   `edgellm_tests`.
4. For hardware: copy `examples/02_WiFiHealthCheck/arduino_secrets.h.example` to
   `arduino_secrets.h` and fill in WiFi credentials (gitignored).

## Build & flash an example

PlatformIO (ESP32 shown):

```bash
pio ci --project-conf platformio.ini -e esp32dev --lib="." examples/01_SelfTest
```

Arduino IDE: open the `.ino`, select your board, install **ArduinoJson**, flash,
open Serial Monitor at 115200.

## Run the test suite

```bash
make test         # native unit tests (host)
make clean        # remove build artifacts
```

CI runs the same suite plus clang-format, cppcheck, gitleaks, and a PlatformIO
compile matrix (`.github/workflows/ci.yml`).

## Refresh the TLS root CA bundle

Roots rotate; refresh on a schedule (suggested: quarterly, and whenever a TLS
handshake starts failing with a verification error).

```bash
python3 tools/gen_ca_bundle.py        # full Mozilla bundle
# or, for tight flash budgets, embed only your provider's root:
python3 tools/gen_ca_bundle.py --file my-provider-root.pem
```

Re-flash devices afterward to pick up the new `RootCABundle.h`.

## Rotate a secret (API key / WiFi password)

1. Issue a new credential with the provider; revoke the old one **after**
   rollout.
2. Update the device store:
   - ESP32 (NVS): call `store.set("anthropic_api_key", newValue)` from a
     provisioning sketch, or use the provisioning portal (Phase 5).
   - In-RAM fallback boards: update `arduino_secrets.h` / your boot provisioning
     and re-flash.
3. Confirm the device reconnects and the old key no longer works.
4. Never log the value; the logger redacts registered secrets, but avoid logging
   them at all.

## Troubleshooting

**TLS handshake / certificate verification fails**
- Confirm NTP time synced (the health-check example logs the epoch). Invalid
  time fails verification closed by design.
- Regenerate the CA bundle (above). Confirm your provider's root is included.
- As a last resort for a *local* endpoint only, `trust().setInsecure()`.

**`HttpBodyTooLarge` error**
- The response exceeded the configured cap. Raise it with
  `http.setMaxResponseBody(bytes)` if you have the RAM, or prefer streaming
  (Phase 2) for large responses.

**`Timeout` on requests**
- Network slow/unreachable, or the server stalled. Increase
  `http.setTimeout(ms)`; verify WiFi signal and DNS.

**Out of memory / crashes on ESP8266**
- ESP8266 sustains roughly one TLS connection at a time. Don't run the (future)
  MCP server and an outbound TLS call concurrently on it; the capability tier
  reflects this.

**Example 02 builds only on ESP32 in CI**
- Intentional for Phase 1. Network examples for other boards land with their HAL
  bring-up in Phase 5; example 01 compiles on all boards.

**Repeated `RateLimited` / `ProviderError` in logs**
- The client already retries 429/5xx with backoff (`client.retryPolicy()`), so
  persistent errors mean a real quota or outage. Check `client.metrics()` —
  `retries`, `httpErrors` and `lastLatencyMs` tell you whether the provider is
  degraded or the device is misconfigured.

**`BudgetExceeded` errors**
- The attached `UsageMeter` cap was reached (deliberate spend guard). Decide:
  raise the cap, or call `meter.reset()` on your schedule (e.g. daily). Caps
  reset on reboot — they are RAM-only counters.

**Device not discoverable by MCP hosts**
- mDNS advertisement (`http.advertise("hostname")`) runs on ESP32/ESP8266 only,
  and only after WiFi is up. Elsewhere connect by IP, or add your core's mDNS
  library. Some networks block multicast — the endpoint still works by IP.

## Watching a deployed device

- **Usage/cost**: attach a `UsageMeter` with `setMaxRequests` /
  `setMaxTotalTokens`; expose `meter.requests()` etc. as an MCP resource or a
  periodic log line.
- **Health**: log `client.metrics()` counters periodically; a rising
  `transportErrors` or `retries` trend flags a degrading network before users
  notice.

## Incident response (sketch)

1. **Detect** — abnormal logs (auth failures, repeated errors), provider billing
   spikes, device offline.
2. **Mitigate** — revoke the affected API key; power down or network-isolate the
   device.
3. **Communicate** — note impact and timeline.
4. **Fix** — rotate secrets, patch, re-flash.
5. **Postmortem** — record root cause and the preventive change.
