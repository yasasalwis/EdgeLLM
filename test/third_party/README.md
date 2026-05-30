# Vendored third-party headers (native tests only)

These files are used **only** to compile the native (host) unit tests. They are
NOT part of the library shipped to devices — on-device, ArduinoJson is pulled in
via the Library Manager / PlatformIO `lib_deps` (see `library.properties` /
`platformio.ini`).

## ArduinoJson/ArduinoJson.h

- Version: **7.4.3**
- Source: https://github.com/bblanchon/ArduinoJson/releases/tag/v7.4.3
  (single-header amalgamation asset `ArduinoJson-v7.4.3.h`)
- License: MIT (© Benoit Blanchon)

To update: download the matching single-header asset for the desired release and
replace the file, keeping the path `ArduinoJson/ArduinoJson.h`.
