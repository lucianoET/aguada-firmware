# Project Research Summary

**Project:** Aguada — GPS Vehicle/Boat Tracker (no-GSM, opportunistic sync) extension
**Domain:** ESP32 embedded IoT tracker — offline-first flash logging + multi-channel opportunistic sync into an existing ESP-NOW/MQTT/Home Assistant telemetry system
**Researched:** 2026-08-01
**Confidence:** MEDIUM

## Executive Summary

This milestone extends the existing Aguada ESP-NOW/MQTT/HA telemetry system with a new node type: a battery-backed GPS tracker for a vehicle/boat that never loses trajectory data and syncs opportunistically over three independent channels — a phone hotspot (live), a known WiFi network (bulk store-and-forward), and the existing Aguada ESP-NOW mesh (position relay with no WiFi at all). Unlike commercial trackers (always-on cellular) or the DIY reference projects consulted (either live-only or logging-only), this design deliberately combines full offline durability with opportunistic live/batch sync and no GSM modem, which is both the differentiator and the primary source of new complexity.

The recommended approach is offline-first: every GPS fix is written to flash (LittleFS, append-only fixed-size binary records with a segment+cursor design, not SPIFFS or a naive ring buffer) before any network attempt, and the three sync channels are consumers of that durable log rather than the primary path. Stack choices lean on proven, already-used-in-repo libraries (TinyGPSPlus, ArduinoJson, PubSubClient, built-in LittleFS/WiFiMulti/esp_sleep) rather than new dependencies. Architecturally, the single ESP32 radio can only run one of {WiFi-STA-to-hotspot, WiFi-STA-to-known-AP, ESP-NOW-mesh} at a time — this must be an explicit mutually-exclusive state machine designed in from day one, not retrofitted, since it is identified as the single biggest rewrite risk in the whole project.

Key risks cluster around three areas: (1) radio/channel conflicts between ESP-NOW and WiFi-STA, which fail silently rather than with errors; (2) GPS/flash data integrity — fix-quality filtering, flash wear-safe logging, and idempotent store-and-forward with a persisted cursor are all "must get right the first time" since the core value proposition is zero data loss; and (3) power/hardware realities (automotive 12V transients, GPS cold-start after deep sleep without VBAT backup) that only surface in real-vehicle field testing, not on the bench. A related structural risk is that Home Assistant cannot backdate history — batch-synced historical points will cluster at upload time in HA's UI, so GPX export must be treated as the actual source of truth for trajectory review, with HA limited to "live state."

## Key Findings

### Recommended Stack

The stack deliberately minimizes new dependencies, reusing what the existing Aguada codebase already relies on (ArduinoJson ^7.0.0, PubSubClient ^2.8) and adding only what's genuinely new for GPS/flash/opportunistic-WiFi. Full detail in `STACK.md`.

**Core technologies:**
- **TinyGPSPlus (1.0.3a)**: NMEA parsing → lat/lon/speed/course/altitude/date/time — de facto standard, dependency-free, exposes `isValid()`/`age()` needed for fix-quality gating.
- **Built-in LittleFS (arduino-esp32 core)**: flash filesystem for the track log — power-loss-safe, wear-leveled, no external library needed; explicitly preferred over the deprecated SPIFFS.
- **PubSubClient (2.8)**: MQTT publish for live + bulk sync — already used in this repo's gateway envs, sufficient for the tracker's non-latency-sensitive connect→publish→disconnect cycle.
- **WiFiMulti + WiFi.onEvent (built-in)**: opportunistic connect to known SSIDs; event-driven reconnect avoids missing short hotspot windows.
- **ArduinoJson (7.4.3)**: variable-size batch payloads, same version already pinned repo-wide.
- **Raw UBX config bytes (no library)** for one-time GPS module tuning (fix rate, power-save); escalate to SparkFun's u-blox GNSS library only if raw config proves flaky.
- **GPSBabel (host-side)**: GPX conversion happens on the server, not on-device.

### Expected Features

Full detail and priority matrix in `FEATURES.md`. The MVP centers on proving "trajectory never lost, syncs on its own" before building the harder differentiators.

**Must have (table stakes):**
- GPS fix acquisition + lat/lon/speed/altitude/course capture, with fix-loss/gap handling
- Continuous offline trajectory logging to flash surviving power loss/reboot
- GPX export of the recorded track
- HA `device_tracker` via MQTT Discovery
- Battery/power status visibility
- Configurable known-WiFi credentials (reuse existing captive-portal/NVS pattern)
- Graceful WiFi/MQTT reconnect with backoff
- Deep sleep / low-power idle when vehicle is off

**Should have (competitive differentiators):**
- Three independent opportunistic sync channels (hotspot live, known-WiFi batch, ESP-NOW mesh) — the actual novel value proposition, no reviewed product combines all three
- Adaptive logging cadence (moving vs. stationary) to save flash/battery
- ESP-NOW position relay through the existing Aguada mesh (zero WiFi needed at all)
- Hybrid 12V + Li-ion power with automatic switching and charge reporting

**Defer (v2+):**
- GSM/LTE fallback and VPN tunneling — explicitly rejected, contradicts the "opportunistic, trusted-networks-only" design
- On-device live map/web UI, fleet provisioning, geofencing/theft alerts (belongs in HA automations), OTA updates, on-device GPX generation

### Architecture Approach

Full detail in `ARCHITECTURE.md`. The tracker is a new sibling PlatformIO environment (`firmware/gps_tracker/`), not folded into `firmware/node`, sharing only `firmware/shared/protocol.h`. The core architectural insight is that the single ESP32 radio makes the three sync channels *sequential, never concurrent* — this drives a dedicated connectivity state machine as the highest-risk, most-isolated module. GPX conversion and all server-side aggregation stay in `tools/` (bridge.py + a new `gpx_export.py`), mirroring the existing Aguada principle that nodes send raw values and math happens at the bridge.

**Major components:**
1. **GPS Reader** — UART NMEA parsing into a `Fix` struct, exposing validity/age/fix-quality
2. **Track Logger** — append-only segment files + persisted cursor on LittleFS, durable-first before any network attempt
3. **Connectivity State Machine** — owns the single radio, enumerates mutually-exclusive modes (IDLE/SCAN/WIFI_HOTSPOT/WIFI_KNOWN/ESPNOW_MESH/SLEEP)
4. **Uplink modules (MQTT + ESP-NOW)** — shared WiFi/MQTT code for live (CH1) and bulk (CH2); new `PKT_GPS_POS` (~26-byte) packet type for CH3, following the existing `PKT_AIR_QUALITY` extension precedent rather than smuggling lat/lon into the 16-byte v3 struct
5. **bridge.py GPS ingest module** — new handler dedup'ing by seq/id across all three inbound paths, driving HA MQTT Discovery `device_tracker` and a server-side history store feeding GPX export

### Critical Pitfalls

Full detail (8 pitfalls) in `PITFALLS.md`. Top risks, ranked by how early they must be designed for:

1. **ESP-NOW/WiFi-STA radio channel conflict** — the two cannot run concurrently on arbitrary channels; joining a hotspot silently breaks mesh delivery with no error reported. Must be solved via an explicit mutually-exclusive state machine before any sync-channel logic is built, not retrofitted.
2. **Flash wear-out / data loss from naive logging** — SPIFFS or JSON-array-in-file logging causes corruption/slowdown or high wear; use LittleFS + append-only fixed-size binary records with segment rotation and a cursor, decided before adaptive cadence or sync logic.
3. **Store-and-forward cursor loses/duplicates points on crash** — cursor must be persisted to flash and only advanced after MQTT ack (QoS1) with a monotonic id + server-side dedupe; this is "the one shortcut this project cannot afford."
4. **NMEA fix-quality filtering omitted** — logging every NMEA sentence without checking fix validity/HDOP/sat-count pollutes the log with null-island/jump artifacts, especially on a boat (multipath) — must gate before any log write.
5. **No VBAT backup → cold-start every wake** — undermines the entire deep-sleep power budget; a ~10-15µA backup source is trivial versus the 10-40x TTFF penalty otherwise.
6. **HA cannot backdate history** — batch-synced historical points cluster at upload time in HA's UI; GPX/external store must be the source of truth for historical review, HA limited to live state — must be an explicit design decision, not a late discovery.

## Implications for Roadmap

Based on combined research, suggested phase structure (dependency-ordered, matching FEATURES.md's MVP/v1.x/v2+ split and PITFALLS.md's "prevention phase" mapping):

### Phase 1: GPS Acquisition & Fix-Quality Filtering
**Rationale:** Every downstream feature (logging, cadence, sync, HA display) depends on trustworthy fixes; PITFALLS.md flags fix-quality gating as needed before any log write.
**Delivers:** UART NMEA parsing (TinyGPSPlus), fix validity/HDOP/sat-count gating, outlier/speed-implausibility rejection.
**Addresses:** Table-stakes "GPS fix acquisition" and "lat/lon/speed/altitude/course capture" from FEATURES.md.
**Avoids:** Pitfall 4 (garbage fixes polluting the log).

### Phase 2: Durable Flash Logging (Track Logger)
**Rationale:** The core value statement ("trajectory never lost") must be solid before any sync channel is built on top of it; storage format is a foundational contract for the exporter and cursor.
**Delivers:** LittleFS append-only segment files, fixed-size binary `FixRecord` struct, segment rotation, persisted cursor.
**Uses:** Built-in LittleFS from STACK.md.
**Implements:** Track Logger component from ARCHITECTURE.md (Pattern 5: segment+cursor, not naive ring buffer).
**Avoids:** Pitfall 2 (flash wear/corruption from naive logging).

### Phase 3: Adaptive Cadence
**Rationale:** Depends on reliable speed field from Phase 1; needed before credible flash/battery budget claims can be made for a real trip.
**Delivers:** Speed-based (not distance-delta) movement classifier with debounce window.
**Addresses:** Differentiator "adaptive logging cadence" from FEATURES.md.
**Avoids:** Pitfall 7 (false-positive movement from GPS jitter while stationary).

### Phase 4: Store-and-Forward Sync (Channel 2 — Known WiFi)
**Rationale:** Simplest channel to prove end-to-end sync works, no live-hotspot timing dependency; FEATURES.md places this as the first sync channel to ship (v1).
**Delivers:** WiFi-STA to known SSIDs, cursor-driven backlog walk, chunked bulk MQTT/HTTP upload, ack-gated cursor advance, monotonic-id dedupe on the bridge.py side.
**Uses:** WiFiMulti + PubSubClient + ArduinoJson from STACK.md.
**Avoids:** Pitfall 3 (duplicate/lost points on crash mid-batch).

### Phase 5: HA Integration (device_tracker + Discovery)
**Rationale:** Validates the HA integration pattern once Phase 4 delivers real data; should be designed to explicitly separate "live state" from "historical trajectory" from the start.
**Delivers:** MQTT Discovery config for `device_tracker`, `json_attributes_topic`-driven live position, battery/power attributes, staleness indicator.
**Addresses:** Table-stakes HA `device_tracker` and battery status from FEATURES.md.
**Avoids:** Pitfall 8 (HA cannot backdate history — batch sync must not touch live device_tracker state history).

### Phase 6: GPX Export (server-side)
**Rationale:** Independent of sync-channel work landing first (GPX can be generated purely from the flash log/bridge history); validates the "trajectory usable outside HA" requirement.
**Delivers:** `tools/gpx_export.py`, converting ingested track history to GPX with the same fix-quality/outlier filtering as the live path.
**Addresses:** Table-stakes GPX export from FEATURES.md.

### Phase 7: Connectivity State Machine — Radio Arbitration
**Rationale:** Highest-risk, most-reusable logic; PITFALLS.md and ARCHITECTURE.md both flag this as the single biggest rewrite risk if retrofitted, so it must be introduced explicitly once the individual channels (WiFi in Phase 4) already work, before Channel 1 and Channel 3 are added.
**Delivers:** Mutually-exclusive mode enum state machine (IDLE/SCAN/WIFI_HOTSPOT/WIFI_KNOWN/ESPNOW_MESH/SLEEP), explicit radio teardown/reinit between modes.
**Implements:** Architecture Pattern 1 from ARCHITECTURE.md.
**Avoids:** Pitfall 1 (silent ESP-NOW/WiFi channel conflict).

### Phase 8: Live Hotspot Sync (Channel 1) + ESP-NOW Mesh Relay (Channel 3)
**Rationale:** Both are v1.x/P2 per FEATURES.md, gated on the state machine (Phase 7) existing; Channel 3 additionally requires the protocol v3 extension.
**Delivers:** Live MQTT publish on hotspot connect (CH1); new `PKT_GPS_POS` (0x30-ish) packet type + gateway pass-through + bridge.py handler (CH3).
**Uses:** New packet type per ARCHITECTURE.md Pattern 3 (not smuggled into the 16-byte v3 struct).
**Avoids:** Pitfall 1 (channel arbitration, now tested against the hotspot's arbitrary channel vs. mesh's fixed channel 1).

### Phase 9: Power Architecture (VBAT, Deep Sleep, Automotive Supply)
**Rationale:** PITFALLS.md explicitly recommends this be co-designed with GPS acquisition (Phase 1) for the hot-start decision, but the full deep-sleep/automotive-hardware validation is a hardware-integration milestone that benefits from the software behavior (Phases 1-8) already being stable; treat as parallel-track hardware work validated last, per FEATURES.md's v1.x placement of "deep sleep" (don't let power optimization block functional validation).
**Delivers:** VBAT-backed GPS hot-start, deep-sleep wake on motion/timer, automotive-rated buck converter + transient protection, real-vehicle brownout/crank testing.
**Avoids:** Pitfall 5 (cold-start every wake) and Pitfall 6 (automotive transients killing the regulator/GPS RF front-end).

### Phase Ordering Rationale

- Software phases (1-6) are ordered by strict data-dependency: fix quality → durable storage → cadence → sync → HA display → export, matching the dependency graph in FEATURES.md almost exactly.
- The connectivity state machine (Phase 7) is deliberately placed *after* Channel 2 (Phase 4) proves basic WiFi-STA sync works in isolation, but *before* Channel 1 and Channel 3 are added — this avoids ever building an "always dual-connected" assumption that would require a rewrite, per the Anti-Pattern 1 warning in ARCHITECTURE.md.
- Power/hardware work (Phase 9) is placed last among planning phases not because it's low-value, but because PITFALLS.md and FEATURES.md agree it's validated in the real vehicle/enclosure and should not gate the functional software validation loop.
- HA integration (Phase 5) is placed before Channel 1/3 sync additions so the "live state vs. historical trajectory" separation (Pitfall 8) is architecturally established before more sync paths start feeding it.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 7 (Connectivity State Machine):** ESP-NOW/WiFi coexistence behavior is MEDIUM-confidence (official ESP-IDF docs but not benchmarked against this exact 3-channel scenario) — needs `/gsd-plan-phase --research-phase` to validate actual channel-switch timing and `esp_wifi_set_channel()` edge cases before committing to the state machine design.
- **Phase 8 (ESP-NOW packet extension + Channel 1/3):** New packet type design touches the existing protocol v3 contract (`firmware/shared/protocol.h`) — low risk technically (precedent exists via `PKT_AIR_QUALITY`) but needs careful review against `AGUADA_SYSTEM_DOC.md` to avoid drift.
- **Phase 9 (Power Architecture):** Automotive supply protection and VBAT/hot-start details are LOW-confidence (aggregated web search, no single authoritative source) — needs component-level research (specific buck converter part numbers, TVS diode selection) during planning, ideally validated with real hardware.

Phases with standard patterns (skip research-phase):
- **Phase 1 (GPS Acquisition):** TinyGPSPlus usage is HIGH-confidence, official docs, well-trodden pattern.
- **Phase 2 (Flash Logging):** LittleFS append+cursor pattern is well-documented and already analogous to existing embedded logging practice.
- **Phase 4 (Store-and-Forward):** PubSubClient/WiFiMulti usage mirrors patterns already implemented elsewhere in this repo.
- **Phase 5 (HA Integration):** MQTT Discovery `device_tracker` is officially documented by Home Assistant with a clear schema.
- **Phase 6 (GPX Export):** Server-side conversion is a straightforward, well-understood task.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH-MEDIUM | Core libraries (TinyGPSPlus, ArduinoJson, PubSubClient) verified via Context7/official docs and GitHub releases API; power-hardware and GPX-pattern claims are MEDIUM/LOW, cross-checked across community sources only |
| Features | MEDIUM-HIGH | Reference projects (Car-Assistant, har-in-air) verified directly as primary sources; the "no-GSM opportunistic tracker" combination itself is synthesized from adjacent domains, not a single existing product |
| Architecture | MEDIUM | Aguada-internal facts (protocol.h, AGUADA_SYSTEM_DOC.md, PROJECT.md) are HIGH-confidence primary sources; radio-coexistence facts are MEDIUM (official ESP-IDF docs, not benchmarked); general store-and-forward/HA patterns are LOW-sourced web search but corroborate well-known engineering practice |
| Pitfalls | MEDIUM-HIGH | Grounded in ESP-IDF docs, u-blox docs, and ESP32 forum consensus for GPS/radio issues; project-specific integration risk (how these pitfalls interact with Aguada specifically) is architecture judgment, not independently benchmarked |

**Overall confidence:** MEDIUM

### Gaps to Address

- **ESP-NOW/WiFi channel-switch timing and reliability:** no direct benchmark of how long/reliable the mode transition is in practice for this exact hardware — validate empirically early in Phase 7 with a real phone hotspot on a differing channel.
- **Automotive power hardware specifics:** buck converter part selection, transient protection component values, and real-vehicle brownout behavior are LOW-confidence web-search-derived — treat as a hardware spike requiring its own component research pass, not just firmware planning.
- **Actual flash wear budget:** the pitfalls research flags this as a "capacity-planning task, not a guess" — needs a concrete calculation (erase-cycle budget vs. chosen cadence and partition size) during Phase 2 planning, not assumed from research alone.
- **VBAT/hot-start behavior on the specific NEO-6M/M8N module actually sourced:** TTFF figures cited are general u-blox datasheet/community figures — verify against the actual module once hardware is in hand.
- **GPS antenna/mounting real-world fix quality (parking garage, marina, engine-running RF noise):** all filtering/cadence logic assumptions should be field-validated in the real mounting location, not just bench-tested, per the "Looks Done But Isn't" checklist in PITFALLS.md.

## Sources

### Primary (HIGH confidence)
- `firmware/shared/protocol.h`, `AGUADA_SYSTEM_DOC.md`, `.planning/PROJECT.md` (repo inspection) — existing protocol v3, packet types, ANI-01 extension precedent, milestone scope/constraints
- `mikalhart/tinygpsplus`, `bblanchon/arduinojson` (Context7 + GitHub Releases API) — library APIs and current versions
- Home Assistant MQTT device_tracker docs (https://www.home-assistant.io/integrations/device_tracker.mqtt/) — Discovery schema
- ShonP40/Car-Assistant and har-in-air/ESP32_IMU_BARO_GPS_VARIO (GitHub, user-cited references) — competitor/reference architecture
- ESP-IDF official docs (Wi-Fi API reference, File System Considerations, Wear Levelling API) via Context7 — radio/filesystem behavior

### Secondary (MEDIUM confidence)
- ESP32 Forum threads on ESP-NOW/WiFi channel conflicts and SPIFFS wear
- u-blox NEO-6M/M8N datasheets and community TTFF/hot-start discussions
- ESPHome GPS component docs (adjacent framework, same HA integration target)

### Tertiary (LOW confidence)
- Aggregated web search on automotive 12V power protection, load-dump transients, GPX generation approaches, WiFiMulti reconnect behavior — used for pattern validation only, not for Aguada-specific decisions; flagged for field/hardware validation during planning

---
*Research completed: 2026-08-01*
*Ready for roadmap: yes*
