# Stack Research

**Domain:** ESP32 GPS vehicle/boat tracker — offline track logging + opportunistic WiFi/ESP-NOW sync (no GSM), integrated into existing Aguada ESP-NOW/MQTT/HA system
**Researched:** 2026-08-01
**Confidence:** MEDIUM (library choices HIGH-confidence via official docs/repos; power-hardware and GPX-pattern claims are MEDIUM/LOW — no single authoritative source, cross-checked across multiple community references)

This document only covers what's **new** for the GPS tracker node. Everything already validated in the Aguada system (ESP-NOW mesh, protocol v3, gateway, `bridge.py`, MQTT/HA Discovery) is out of scope — see `AGUADA_SYSTEM_DOC.md`.

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| **TinyGPSPlus** | `1.0.3a` (mikalhart/TinyGPSPlus) | NMEA parsing → lat/lon/speed/course/altitude/date/time | The de facto standard Arduino GPS library — tiny, dependency-free, character-streaming `encode()` API that works identically on NEO-6M and NEO-M8N. Every field exposes `isValid()`/`isUpdated()`/`age()`, which maps directly onto the "only log a fix that's fresh and valid" requirement. Confidence: HIGH (official repo + Context7 docs). |
| **arduino-esp32 built-in LittleFS** | ships with arduino-esp32 core ≥2.0.x (project already targets recent espressif32 platform) | Flash filesystem for the track log | LittleFS is now part of the core itself — no external library needed (the old `lorol/LITTLEFS` fork is superseded/deprecated). Power-loss-safe, wear-leveled, and already the recommended choice for all new ESP32 projects using flash storage. Confidence: MEDIUM (cross-checked across Espressif docs + community sources, no single canonical announcement). |
| **PubSubClient** | `2.8` (knolleary/pubsubclient) | MQTT publish over WiFi hotspot/known-SSID (Canal 1 live, Canal 2 batch dump) | Already the MQTT client used in this repo's `gateway-esp32-wifi`/`gateway-esp32-dual` envs — reuse it for consistency and zero new dependency risk. It's synchronous/blocking, which is *fine* here because the tracker's WiFi connect→publish→disconnect cycle is not latency-sensitive and doesn't need to stay responsive to anything else during the publish burst. Confidence: MEDIUM. |
| **WiFiMulti** (arduino-esp32 built-in) | ships with core | Register known SSIDs (phone hotspot + home/office WiFi), connect to strongest available | Zero extra dependency, directly expresses "opportunistic connect to any known network." Must be paired with `WiFi.onEvent(WIFI_EVENT_STA_DISCONNECTED, ...)` because WiFiMulti alone has documented issues re-joining an AP that dropped and came back (e.g., phone hotspot toggled off/on). Confidence: LOW-MEDIUM (community reports, not official spec). |
| **ArduinoJson** | `7.4.3` (bblanchon/ArduinoJson, already pinned `^7.0.0` in this repo) | Building the MQTT JSON payloads / HTTP batch dump | Already the project standard (gateway uses it). v7's single heap-allocated `JsonDocument` (no more `StaticJsonDocument<N>` capacity guessing) is simpler for variable-size batch payloads (a store-and-forward dump has a variable number of points). Confidence: HIGH (Context7-verified). |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| **Raw UBX config bytes (no library)** | n/a — hand-rolled byte arrays + checksum | One-time boot-time GPS module tuning: set fix rate (`UBX-CFG-RATE`, e.g. 1–2 Hz is plenty for a vehicle tracker), disable unneeded NMEA sentences (keep only `RMC`+`GGA`), and (M8N only) request `UBX-CFG-PMS`/`CFG-PM2` power-save mode | Default choice. Keeps flash footprint minimal — you don't need a full UBX parser, just fire-and-forget config sentences at boot (with checksum) over the same UART. This is standard practice in embedded GPS trackers; see "What NOT to Use" for when to upgrade to a full UBX library instead. |
| **SparkFun u-blox GNSS Arduino Library** | latest (`sparkfun/SparkFun_u-blox_GNSS_Arduino_Library`) | Full bidirectional UBX protocol with ACK/NAK handling, per-message callbacks | Only if raw NEO-6M UBX-CFG-RXM power-save proves flaky in testing, or if the M8N's ACK/NAK handshake for `CFG-PMS` needs to be verified programmatically rather than assumed. Adds real flash/RAM overhead — don't pull it in unless the raw-bytes approach fails in the field. |
| **WiFi.onEvent / esp_wifi events** (arduino-esp32 built-in) | ships with core | Detect disconnect/connect transitions to drive the "flush store-and-forward log" state machine | Use instead of polling `WiFi.status()` in `loop()` — event-driven reconnect avoids missing a short hotspot window (e.g., phone hotspot only up for 30s). |
| **esp_sleep (ESP-IDF, via Arduino `esp_sleep_enable_timer_wakeup()`/`esp_deep_sleep_start()`)** | ships with core | Deep sleep between GPS reads when stationary | Central to the hybrid-power requirement. Timer-wakeup deep sleep is sufficient for this milestone (no accelerometer in scope); wake, take a fix, log if moved ≥ threshold, sleep again. |
| **MAX1704x fuel-gauge library (e.g. `adafruit/Adafruit MAX1704X`)** | latest, optional | Li-ion state-of-charge reporting to HA (battery %, voltage) | Optional nice-to-have, not required for this milestone's scope (12V+Li-ion backup + deep sleep only). Add only if a fuel-gauge IC is actually placed on the board; a simple ADC voltage divider read is a zero-dependency fallback. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| **PlatformIO** | Build/flash/monitor, same as `firmware/node` and `firmware/gateway` | New `platformio.ini` env for the tracker target (e.g. `[env:gps-tracker-c3]` or `[env:gps-tracker-esp32]`), following the existing repo convention of per-device envs with `build_flags` defaults + `partitions.csv`. |
| **GPSBabel** (host-side, not on-device) | Convert logged track (binary/CSV) to GPX for Google Earth / external analysis | Runs on the Linux server alongside `bridge.py`, not on the ESP32 — keeps the on-device code simple (log compact binary, don't build XML strings on a microcontroller). |

---

## Installation

```ini
; firmware/gps-tracker/platformio.ini (new target)
[env:gps-tracker-c3]
platform  = espressif32
board     = esp32-c3-devkitm-1
framework = arduino

board_build.partitions = partitions.csv   ; reserve a data partition for LittleFS track log
board_build.filesystem = littlefs

monitor_speed = 115200
upload_speed  = 921600

build_flags =
    -DCORE_DEBUG_LEVEL=3
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -Iinclude
    -I../shared

lib_deps =
    mikalhart/TinyGPSPlus @ ^1.0.3
    knolleary/PubSubClient @ ^2.8
    bblanchon/ArduinoJson @ ^7.0.0
```

No `lib_deps` entry is needed for LittleFS, WiFiMulti, or `esp_sleep` — all ship inside the `espressif32`/arduino-esp32 core already used by every other env in this repo.

---

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|--------------|-------------|--------------------------|
| TinyGPSPlus | NeoGPS | If RAM becomes tight (unlikely on ESP32-C3/ESP32 classic with only NMEA parsing) or you need compile-time sentence filtering to shave cycles. NeoGPS is more memory-efficient but has a steeper, less-documented API and a smaller community — not worth the switch unless a concrete RAM/CPU constraint appears. |
| TinyGPSPlus + raw UBX config | SparkFun u-blox GNSS Arduino Library | If you need full bidirectional UBX ACK/NAK handling, high-precision (RTK) messages, or config that must be verified rather than assumed. Overkill for "read NMEA fix + tune update rate/power-save once at boot." |
| arduino-esp32 built-in LittleFS | lorol/LITTLEFS fork | Only on very old arduino-esp32 core versions (<2.0) that predate the built-in LittleFS support. Not applicable here — this repo already targets recent `espressif32` platform releases. |
| PubSubClient | AsyncMqttClient (ESP32Async maintained fork) | If the tracker needs to stay responsive to *other* work (e.g., serving a captive-portal config page) while an MQTT publish is in flight. The original `marvinroger/async-mqtt-client` is unmaintained since 2021 — if async is genuinely needed, use the `ESP32Async` org's maintained fork, not the original. For this milestone's simple "connect → dump → disconnect" flow, the added complexity isn't justified. |
| WiFiMulti | WiFiManager | If the tracker needs a captive-portal UI for entering new SSID/password on-device (this project's AirQ node and `rfid_test` already implement a hand-rolled captive portal with built-in `WebServer`+`DNSServer`, no external library — follow that existing pattern rather than adding WiFiManager as a new dependency, unless the config UX genuinely needs WiFiManager's flow). |
| Binary fixed-size log records + host-side GPX conversion | On-device GPX (XML) generation | If a real-time downloadable GPX file is a hard requirement of *this* milestone (it isn't — HA `device_tracker` + later export covers it). Building XML strings on a microcontroller wastes RAM/flash-writes versus a compact binary record; convert to GPX with GPSBabel on the server when needed. |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|--------------|
| SoftwareSerial for the GPS UART on ESP32 | ESP32 (both classic and C3) has multiple hardware UARTs — `HardwareSerial` is more reliable at GPS baud rates (9600–38400) and doesn't block interrupts the way bit-banged SoftwareSerial can. SoftwareSerial is an AVR-era workaround, not needed here. | `HardwareSerial` (e.g. `Serial1`/`Serial2`) mapped to any free GPIIO pair via `.begin(baud, SERIAL_8N1, rxPin, txPin)`. |
| SPIFFS for new flash storage | Deprecated in favor of LittleFS across the ESP32 Arduino ecosystem; slows down past ~70% partition fill due to its garbage collector, no directory support, worse wear-leveling. | LittleFS (built into arduino-esp32 core). |
| Raw/unformatted SPI flash partition access (writing directly to flash offsets, bypassing a filesystem) | No wear-leveling, no crash safety, and reinvents what LittleFS already does well. Only justified if you need a true fixed-size circular buffer with byte-level control and are willing to hand-roll wear-leveling — not worth it for this milestone's scope. | LittleFS with fixed-size binary records (append-only file, rotate/truncate when full). |
| marvinroger/async-mqtt-client (original, unmaintained since 2021) | Known reliability issues: crashes on large inbound packets, weak QoS1/2 recovery, no active maintenance against current ESP32 cores. | PubSubClient (sync, sufficient here) or the actively-maintained `ESP32Async` fork if async is truly required later. |
| Text/CSV track logging on-device | Larger per-point flash footprint (ASCII vs packed binary), more flash-write wear over a long trip, and string formatting costs cycles/RAM that a battery-backed MCU can't spare. | Fixed-size packed binary struct per trackpoint (e.g. `{uint32_t ts; int32_t lat_e7; int32_t lon_e7; int16_t alt_dm; uint8_t speed_kmh; uint16_t course_deg;}` — 17 bytes/point), converted to GPX/CSV only on export. |
| Building a full on-device web map/GPX viewer | Explicitly out of scope per `PROJECT.md` — visualization lives in Home Assistant; captive portal only if time allows. | HA `device_tracker` via MQTT Discovery + host-side GPX export. |

---

## Stack Patterns by Variant

**If the tracker board is ESP32-C3 (matches existing node fleet, single hardware UART budget is tighter):**
- Use the native USB-CDC serial for debug/logging (as already done in `firmware/node`), and dedicate the one flexible UART pair to the GPS module.
- LittleFS partition should be modest (track log for a multi-hour trip at 17 bytes/point and ~1 point/5-10s is small — even 1MB holds tens of thousands of points).

**If the tracker board is classic ESP32 (more GPIO/UART headroom, matches `esp32-devkit-relay` env already in this repo):**
- Same libraries apply unchanged; classic ESP32 has 3 hardware UARTs, so GPS + a future NEO-M8N config channel + debug serial can all be dedicated pins without contention.

**If Canal 3 (ESP-NOW to Aguada mesh) needs to carry lat/lon:**
- The existing 16-byte protocol v3 packet has no room for a `float`/`int32` lat/lon pair — this requires a **new packet type** (not a v3 field addition) per the `Key Decisions` already logged in `PROJECT.md`. Recommend a compact fixed-point encoding (`int32_t lat_e7, lon_e7` = 8 bytes) inside a new `0x30 POSITION` (or similar) packet type, kept structurally consistent with the existing packed/CRC-16 pattern — this is a protocol-design decision for the planning phase, not a library choice.

---

## Version Compatibility

| Package A | Compatible With | Notes |
|-----------|------------------|-------|
| `mikalhart/TinyGPSPlus @ ^1.0.3` | any `espressif32` platform version already used in this repo | Pure C++ NMEA parser, no ESP32-core dependency beyond `Stream`/`HardwareSerial`. |
| `bblanchon/ArduinoJson @ ^7.0.0` | already pinned in `firmware/gateway/platformio.ini` | Reuse the same version pin across the tracker target to avoid divergent JSON payload shapes/behavior. |
| `knolleary/PubSubClient @ ^2.8` | already pinned in `firmware/gateway/platformio.ini` (`gateway-esp32-wifi`, `gateway-esp32-aguada-web` envs) | Note the ~1KB practical payload ceiling — plan the store-and-forward batch dump to chunk into multiple MQTT messages (or use HTTP POST for the bulk dump instead of MQTT) rather than raising `setBufferSize()` far past a few KB. |
| Built-in LittleFS | requires `board_build.filesystem = littlefs` in `platformio.ini` and a partition table with a data/spiffs region sized appropriately | Must add/adjust `partitions.csv` for the new tracker env — this repo's existing `partitions.csv` files are sized for the sensor/gateway use case, not a multi-hour track log; size the filesystem partition explicitly for the tracker target. |

---

## Sources

- `mikalhart/tinygpsplus` (Context7) — API usage (encode/location/speed/course/altitude), confirmed latest release `v1.0.3a` via GitHub API. Confidence: HIGH.
- `bblanchon/arduinojson` / `websites/arduinojson` (Context7) — JsonDocument v7 API, confirmed latest release `v7.4.3` via GitHub API. Confidence: HIGH.
- GitHub Releases API (`mikalhart/TinyGPSPlus`, `bblanchon/ArduinoJson`, `lorol/LITTLEFS`, `knolleary/pubsubclient`, `marvinroger/async-mqtt-client`) — version/maintenance-status verification. Confidence: HIGH (direct API, not search-summarized).
- Web search: TinyGPSPlus vs NeoGPS vs SparkFun u-blox GNSS library comparison. Confidence: LOW (aggregated community consensus, no single canonical source).
- Web search: LittleFS vs SPIFFS/raw flash for ESP32 logging. Confidence: LOW-MEDIUM (Espressif docs referenced within results + community sources).
- Web search: PubSubClient vs AsyncMqttClient reliability/payload limits, and ESP32Async maintained-fork status. Confidence: LOW-MEDIUM.
- Web search: ESP32 WiFiMulti reconnect behavior and event-driven pattern. Confidence: LOW.
- Web search: ESP32 deep sleep + Li-ion + 12V vehicle hybrid power patterns (Valtrack V4 reference design, TPS54240/MP2617-class ICs). Confidence: LOW.
- Web search: GPX generation approaches (on-device vs post-process with GPSBabel). Confidence: LOW.
- Web search: u-blox NEO-6M/NEO-M8N UBX vs NMEA, power-save config (`UBX-CFG-RXM`/`CFG-PMS`/`CFG-PM2`). Confidence: LOW-MEDIUM (cross-referenced with u-blox datasheet PDF titles surfaced in results).
- Repo inspection: `firmware/gateway/platformio.ini`, `firmware/node/platformio.ini`, `firmware/rfid_test/platformio.ini` — confirmed existing library pins (ArduinoJson ^7.0.0, PubSubClient ^2.8) and the hand-rolled (no-library) captive-portal pattern already in use for the AirQ/RFID nodes. Confidence: HIGH (primary source, this repo).

---
*Stack research for: ESP32 GPS vehicle/boat tracker (offline logging + opportunistic WiFi/ESP-NOW sync)*
*Researched: 2026-08-01*
