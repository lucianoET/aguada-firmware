# Walking Skeleton — Aguada GPS Tracker (sem GSM)

**Phase:** 1
**Generated:** 2026-08-01
**Status:** Skeleton is **already built and hardware-validated** at bench level (commit `540e11b`, 2026-08-01). Phase 1's plans evolve it — they do not re-create it.

## Capability Proven End-to-End

> A bench operator powers an ESP32 + u-blox/AT6558 GPS module over USB and watches a real satellite fix — latitude, longitude, altitude, speed, course and UTC — appear on the serial monitor at 115200.

This already works. Achieved on an ESP32-C3 SuperMini + NEO-6M at latitude -22.919952, longitude -43.215005. The full stack that matters for this project (GPS module → UART → NMEA parser → application code → operator-visible output) is proven end to end, on real hardware, with real satellites.

Phase 1's two plans extend that proven path with the two decision layers every later phase depends on: quality gating (`fix_gate`) and adaptive cadence (`cadence`).

## Architectural Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Project location | `firmware/gps_tracker/` — standalone PlatformIO project, sibling to `firmware/node/` and `firmware/gateway/` | D-05. Not an env inside `firmware/node`: the tracker's dependency set, partition needs (LittleFS, Phase 2) and lifecycle diverge from the sensor node's. Shares `firmware/shared/protocol.h` only if v2 ESP-NOW position relay lands. |
| Target board | ESP32 DevKit classic (WROOM-32, 30 pins) is the **target**; ESP32-C3 SuperMini is the **current bench board** | D-01 chose the DevKit for pin headroom — the full v2 peripheral set (TFT, IMU, HTU21D, buttons, NMEA-out for marine radar) does not fit on the C3, which the pinout doc records as 100% allocated. Both envs stay building for the whole milestone; every plan verifies both. |
| GPS module | ATGM336H (AT6558, GPS+BeiDou) **and** NEO-6M (u-blox), interchangeable on one binary | D-02. Firmware is NMEA-agnostic: no UBX, no CASIC, no PMTK, no baud probing, no proprietary power-save. The common denominator is plain NMEA. |
| GPS link | UART, 9600 baud, 1 Hz, hardware serial, receive-only | D-03/D-04. DevKit: UART2, GPIO16 (RX) / GPIO17 (TX), LED GPIO2. C3 SuperMini: UART1, GPIO20 (RX) / GPIO21 (TX), LED GPIO8. Canonical map is `firmware/gps_tracker/gps_tracker_pinout.md`. Never SoftwareSerial. |
| NMEA parsing | `mikalhart/TinyGPSPlus @ ^1.0.3` (resolves 1.1.0) | Already vendored, already hardware-validated in this project, audited OK in `01-RESEARCH.md`. Handles multi-talker IDs, checksum validation, and corrupted-sentence recovery — the bench's 10-17% checksum-failure rate from an intermittent TX wire is discarded cleanly with no crash and no garbage values. No hand-rolled NMEA parsing anywhere. |
| Module boundaries | `gps_reader` → `fix_gate` → `cadence`, wired in `main.cpp` | Each layer is a pure step over the previous one's output. `gps_reader` owns the UART and the parser and nothing else; `fix_gate` decides accept/reject; `cadence` decides emit/suppress. Mirrors the existing `firmware/node/src/sensor_filter.*` separation of raw read from filtering decision. |
| Inter-module contract | An immutable `Fix` snapshot struct (POD), defined in `include/gps_types.h` | The gate evaluates a snapshot, not live parser state. This makes the gate a pure function (testable without hardware) and removes the ordering hazard created by the GGA fix-quality accessor consuming the parser's updated flag. This deviates from `01-RESEARCH.md` Pattern 2's `evaluate(TinyGPSPlus&)` signature, deliberately and more strictly. |
| Configuration | `#ifndef DEFAULT_GPS_*` / `#define` in `include/gps_config.h`, overridable by `platformio.ini` `build_flags` | Copies the established repo convention from `firmware/node/include/node_config.h`. Single source of truth for defaults; no duplicated `-D` lines to drift. Field tuning never requires a source edit. |
| Timekeeping | `utc_unix` (seconds since epoch, from GPS date+time) for records; `mono_ms` (`millis()`) for all interval arithmetic | GPS UTC jumps discontinuously when the receiver first acquires the date. Anything that measures an interval must use the monotonic stamp or it will misfire once per cold start. |
| Movement signal | The receiver's own speed field, never position deltas | Stationary position wander on these single-frequency receivers reads as continuous slow motion. Bench observed about 2.6 km/h of stationary speed jitter — the hysteresis band is sized above it. |
| Observability | USB serial 115200 with tagged line prefixes (`[FIX]`, `[ACCEPT]`, `[REJECT]`, `[EMIT]`, `[SUPPRESS]`, `[STATE]`, `[HEALTH]`, `[SIM]`) | Bench-only for this phase. Prefix tagging keeps captured logs machine-greppable and human-readable months later; documented in `firmware/gps_tracker/BENCH.md`. |
| Bench simulation | Serial-command fix injection behind `GPS_BENCH_SIM`, routed through the real gate-and-cadence path | The repo has no automated test suite — validation is on-hardware. Injecting through the shipping code path makes GPS-02 and GPS-03 provable at a desk in minutes instead of requiring a vehicle and a tunnel. Must be set to 0 before any networked build (Phase 3+). |

## Stack Touched in Phase 1

- [x] Project scaffold — `firmware/gps_tracker/platformio.ini`, two envs, dependency pinned, both flash and run **(done at commit `540e11b`)**
- [x] Hardware I/O — GPS UART wired and reading real NMEA **(done, hardware-validated)**
- [x] Real data path — NMEA → parser → real satellite fix on serial **(done, hardware-validated)**
- [x] Operator-visible output — USB serial monitor at 115200 with LED fix status **(done)**
- [ ] Structured `Fix` snapshot contract for downstream phases — Plan 01 Task 1
- [ ] Quality gating before anything downstream — Plan 01 Task 2
- [ ] Adaptive cadence — Plan 02 Task 1
- [ ] Documented bench verification procedure (`BENCH.md`) — Plan 02 Task 2

No deployment step exists or is needed: the full-stack run command is `pio run -e esp32-c3-supermini -t upload --upload-port /dev/ttyACM0 && pio device monitor`.

## Out of Scope (Deferred to Later Slices)

Explicit, so later phases do not re-litigate Phase 1's minimalism:

- **Flash persistence of any kind** — LittleFS, binary records, segment rotation, upload cursor → Phase 2 (LOG-01..03). Phase 1 emits to serial and stops there.
- **Any network** — WiFi, WiFiMulti, MQTT, PubSubClient, HTTP, backlog upload, retry/backoff → Phase 3 (SYNC-01..03).
- **Server side** — `bridge.py` ingest, HA `device_tracker` MQTT Discovery, `tools/gpx_export.py` → Phase 4 (SRV-01..03).
- **Power** — 12V/Li-ion hybrid switching, battery ADC read, source reporting → Phase 5 (PWR-01/02).
- **Peripherals** — TFT ST7735, buttons, buzzer, extra status LEDs, I2C IMU/HTU21D, DS18B20, NMEA-0183 output for marine radar. Pins are reserved and documented in `gps_tracker_pinout.md`; nothing is mounted or driven → HW-01 (Phase 5 documentation) and HW-02..07 (v2).
- **GPS proprietary configuration** — UBX `CFG-RXM`/`CFG-PMS`/`CFG-PM2`, CASIC equivalents, static-hold, fix-rate reconfiguration, baud change. Blocked by D-02: one binary must serve both modules.
- **Deep sleep** — PWR-03, v2. Coupled to GPS power-save above.
- **PPS time synchronisation** from the ATGM336H — v2, unused in v1.
- **Signal-gap marking** (tunnel/bridge produces an explicit gap rather than a teleport) — GPS-04, v2.
- **ESP-NOW / protocol v3 integration** — the 16-byte v3 packet has no room for a lat/lon pair; a new packet type is a v2 decision (SYNC-05).
- **On-device GPX generation or web map** — explicitly out of scope project-wide; HA is the only panel.
- **Automated unit tests** — the repo has no test suite and Phase 1 does not introduce one; the bench simulator plus `BENCH.md` is the validation mechanism.

## Subsequent Slice Plan

Each later phase adds one vertical slice on top of this skeleton without altering the architectural decisions above:

- **Phase 2 — Durable Flash Logging:** consumes the `Fix` struct at the emit point and writes it to LittleFS as a fixed-size binary record before any network step exists. The single `onGatedFix` emit site in `main.cpp` is where the logger hooks in; nothing about `gps_reader`/`fix_gate`/`cadence` should need to change. Requires a `partitions.csv` sized for the track log.
- **Phase 3 — Store & Forward Sync (Known WiFi):** adds WiFiMulti + PubSubClient, uploads the backlog from the persisted cursor. Must set `GPS_BENCH_SIM` to 0. First phase where the tracker has an attack surface.
- **Phase 4 — Server Ingest, HA Live Tracking & GPX Export:** extends `tools/bridge.py` and adds `tools/gpx_export.py`. No firmware architecture change.
- **Phase 5 — Hybrid Power & Hardware Provisioning:** adds the battery ADC read on the reserved pin and reports source/voltage as `device_tracker` attributes; formalises the HW-01 reserved-pin map. This is where the DevKit-versus-C3 board decision (D-01) has to be settled with real hardware, since the C3 has no pin budget left.
