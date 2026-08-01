# Architecture Research

**Domain:** Opportunistic-sync GPS vehicle/boat tracker integrating into an existing ESP-NOW/MQTT reservoir-telemetry system (Aguada)
**Researched:** 2026-08-01
**Confidence:** MEDIUM (system-internal facts HIGH from source read; radio-coexistence facts MEDIUM from official ESP-IDF docs; general store-and-forward/HA patterns LOW-sourced web search, but corroborate well-known engineering practice)

## Standard Architecture

### System Overview

```
┌───────────────────────────────────────────────────────────────────────────┐
│                          GPS TRACKER NODE (ESP32)                          │
├───────────────────────────────────────────────────────────────────────────┤
│  ┌────────────┐   ┌────────────────┐   ┌───────────────────────────────┐ │
│  │ GPS Reader │──▶│ Fix Processor / │──▶│  Track Logger (flash)          │ │
│  │ (NEO-6M/8N │   │ Adaptive Cadence│   │  append-only segment files +   │ │
│  │  UART NMEA)│   │ (moving/parked) │   │  upload cursor                 │ │
│  └────────────┘   └────────────────┘   └────────────┬──────────────────┘ │
│                                                       │                    │
│                                          ┌────────────▼──────────────┐    │
│                                          │   Connectivity State      │    │
│                                          │   Machine (single radio,  │    │
│                                          │   mutually-exclusive modes)│   │
│                                          └──┬───────┬───────┬────────┘    │
│                          ┌──────────────────┘       │       └──────────┐  │
│                          ▼                          ▼                  ▼  │
│                 ┌──────────────┐          ┌──────────────────┐ ┌──────────────┐
│                 │ CH1: Hotspot │          │ CH2: Known WiFi  │ │ CH3: ESP-NOW │
│                 │ WiFi STA →   │          │ STA → bulk MQTT/ │ │ mesh (ch1) → │
│                 │ live MQTT    │          │ HTTP dump of      │ │ position pkt │
│                 │ over internet│          │ backlog segments  │ │ to gateway   │
│                 └──────┬───────┘          └────────┬──────────┘ └──────┬───────┘
│                        │                            │                   │        │
└────────────────────────┼────────────────────────────┼───────────────────┼────────┘
                          │                            │                   │
                          ▼                            ▼                   ▼
                 ┌─────────────────────────────────────────────┐   ┌───────────────┐
                 │           tools/bridge.py (server)            │   │ Existing      │
                 │  MQTT broker client + new GPS ingest handler  │   │ ESP-NOW       │
                 │  - live device_tracker updates                │   │ Gateway       │
                 │  - bulk backlog ingest (dedup by seq/ts)       │◀──│ (ESP32-S3)   │
                 │  - GPX exporter                                │   │ USB JSON     │
                 └───────────────────┬────────────────────────────┘   └───────────────┘
                                     ▼
                          ┌────────────────────┐
                          │  Home Assistant     │
                          │  device_tracker     │
                          │  (map) + GPX file   │
                          └────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| GPS Reader | Parse NMEA/UBX from NEO-6M/M8N over UART, expose fix (lat, lon, alt, speed, course, fix quality, sats) | TinyGPS++ or minmea; hardware UART, 9600 baud default (M8N can go higher) |
| Fix Processor | Decide cadence (5-10s moving, near-stop when stationary), reject fixes without a valid 3D/2D lock, debounce GPS jitter | Distance/time-since-last-fix heuristic; HDOP/sat-count gate |
| Track Logger | Persist fixes durably before any network attempt; track what has been synced | Append-only records in flash (LittleFS/SPIFFS or raw partition), separate small cursor record |
| Connectivity State Machine | Own the single WiFi/ESP-NOW radio; decide which of the 3 channels is active; never assume two are concurrent | FreeRTOS task + explicit enum state machine (IDLE → SCAN → CH1/CH2/CH3 → back to IDLE) |
| Bulk Uploader | Walk unsynced records from cursor, batch into JSON/HTTP or many small MQTT publishes, advance cursor only on ack | Same logic reused for CH1 (live single latest fix) and CH2 (backlog) |
| ESP-NOW Position Packet | Send current fix opportunistically to gateway using new/extended packet path, reusing existing mesh relay | New `PKT_GPS` type or multi-packet split, following the ANI-01 precedent |
| bridge.py GPS Handler | New ingest path parallel to existing SENSOR/HEARTBEAT handling: accepts live MQTT position + bulk backlog + ESP-NOW-forwarded fixes, dedups, republishes `device_tracker` state | Python module added to bridge.py, same process, new MQTT topics |
| GPX Exporter | Convert stored track (server-side, not on the node) into GPX for external tools/Google Earth | Runs on server against bridge.py's ingested history (SQLite/CSV/log), not on-node |
| HA Integration | `device_tracker` via MQTT Discovery, backed by `json_attributes_topic` carrying lat/lon/speed/alt | Standard HA MQTT device_tracker component, same Discovery pattern already used for sensors |

## Recommended Project Structure

```
firmware/
├── gps_tracker/                    # new PlatformIO env, sibling to firmware/node
│   ├── include/
│   │   └── protocol.h              # extends firmware/shared/protocol.h (new PKT_GPS type)
│   └── src/
│       ├── main.cpp                # boot, mode dispatch, deep sleep management
│       ├── gps_reader.cpp/.h        # UART NMEA parsing → fix struct
│       ├── track_logger.cpp/.h      # flash segment writer + cursor
│       ├── conn_state_machine.cpp/.h# WiFi/ESP-NOW mutually-exclusive state machine
│       ├── uplink_mqtt.cpp/.h       # CH1 live + CH2 bulk (shared HTTP/MQTT client code)
│       └── uplink_espnow.cpp/.h     # CH3 position packet using shared protocol.h
├── shared/
│   └── protocol.h                  # add PKT_GPS (0x05) + fields, keep 16B core packet intact
tools/
├── bridge.py                       # add gps ingest module (new MQTT topics + dedup + HA discovery)
├── gps_history.sqlite (or .csv)    # server-side store of ingested track, backing GPX export
└── gpx_export.py                   # new: converts stored track → .gpx on demand
```

### Structure Rationale

- **Separate `firmware/gps_tracker/` PlatformIO env**, not folded into `firmware/node`: the tracker has a fundamentally different runtime shape (GPS UART + flash logger + WiFi bulk uploads + deep sleep) vs. the sensor/relay node's simple measure→filter→send loop. Sharing only `firmware/shared/protocol.h` keeps the existing node firmware untouched (matches the "don't break v3" constraint) while letting the tracker firmware evolve independently — same pattern already used for `rfid_test/` as its own env.
- **`conn_state_machine` as its own module**: this is the highest-risk, most-reusable piece of logic (single radio, 3 mutually exclusive channels) — isolating it means it can be unit-tested/simulated independently of GPS parsing or flash I/O.
- **GPX conversion lives in `tools/`, not on the node**: the node's job is capture + durable local storage + opportunistic delivery of raw fixes; format conversion, joining segments across sync sessions, and any presentation-layer work belongs server-side where compute/storage is cheap and where the full trajectory (potentially reassembled from multiple partial uploads across CH1/CH2/CH3) actually lives. This mirrors the existing pattern where all reservoir math is deliberately kept out of nodes and done in `bridge.py`.
- **`uplink_mqtt.cpp` shared between CH1 (live) and CH2 (bulk)**: both are "WiFi STA + MQTT/HTTP" — the only real difference is *what* gets sent (single freshest fix vs. cursor-driven backlog walk) and which SSID triggered it. One code path, two invocation modes, avoids duplicated WiFi/MQTT client logic.

## Architectural Patterns

### Pattern 1: Single-radio mutually-exclusive connectivity state machine

**What:** ESP32 has one radio and one "home channel." Per ESP-IDF's own WiFi driver docs, in STA/AP-coexistence the connected AP's channel becomes the home channel and takes precedence; if that differs from a fixed ESP-NOW peer channel, the radio migrates and existing ESP-NOW peers on the old channel become unreachable until the radio moves back. Aguada's ESP-NOW mesh is hard-pinned to channel 1. A phone hotspot or a "known WiFi" AP will essentially never also be on channel 1 by design.

**When to use:** Any time a single ESP32 must alternate between "join an arbitrary WiFi AP" and "talk ESP-NOW on a fixed channel." This is exactly the tracker's situation (3 channels, 1 radio).

**Trade-offs:** Cannot get "live MQTT while still reachable over ESP-NOW" — accept that the 3 channels are sequential, not concurrent. This must be designed in from day one; retrofitting it after building "always-on WiFi + always-on ESP-NOW" assumptions is the single biggest rewrite risk in this project (see Pitfalls). Upside: it's a simple deterministic enum state machine, easy to reason about and test (SCANNING → CONNECTING_HOTSPOT / CONNECTING_KNOWN_WIFI / ESPNOW_MESH → back to SCANNING/SLEEP).

**Example:**
```cpp
enum class ConnMode { IDLE, SCAN, WIFI_HOTSPOT, WIFI_KNOWN, ESPNOW_MESH, SLEEP };

ConnMode decideNextMode() {
    if (scanFound(KNOWN_HOTSPOT_SSID)) return ConnMode::WIFI_HOTSPOT;   // live MQTT
    if (scanFound(knownWifiList))       return ConnMode::WIFI_KNOWN;    // bulk dump
    if (espnowPeerHeard())              return ConnMode::ESPNOW_MESH;  // opportunistic position
    return ConnMode::SLEEP;                                            // deep sleep, retry later
}
// Radio is torn down and reinitialized (WiFi.disconnect()+esp_wifi_deinit or channel switch)
// between modes — never assume peers survive a mode transition.
```

### Pattern 2: Store-first, sync-opportunistic (offline-first)

**What:** The device always writes the fix to flash first; every network path (live MQTT, bulk WiFi, ESP-NOW) is purely a consumer of that durable log, never the primary path. This is the standard shape for IoT/asset trackers that must survive arbitrarily long offline periods — sense → store → (whenever a channel exists) → forward, never sense → forward → (fallback) store.

**When to use:** Any tracker where "no point of the trajectory can be lost" is a hard requirement (explicitly true here per PROJECT.md). Also the correct default even for the "live" hotspot channel — CH1 should still write-then-publish, so a live fix is never lost just because the MQTT publish failed.

**Trade-offs:** Slightly more flash wear and code complexity (need a cursor) than "just publish and move on," but it's the only design that survives the deep-sleep/ignition-off reality this tracker lives in.

**Example:**
```cpp
void onNewFix(const Fix& f) {
    trackLogger.append(f);      // always durable-first
    if (connMode == ConnMode::WIFI_HOTSPOT) uplinkMqtt.publishLive(f);
    // CH2/CH3 don't publish per-fix; they walk the log from the cursor later
}
```

### Pattern 3: Multi-packet / new-type extension of a fixed-size binary protocol

**What:** Aguada's existing protocol v3 is deliberately fixed at 16 bytes with a `static_assert`. The codebase already has two precedents for growing beyond this without breaking the 16-byte core: (a) the ANI-01 node splits multi-value telemetry (eCO2/AQI + temp/humidity) across **two packets per cycle**, each still 16 bytes, reusing existing fields with repurposed semantics (`distance_cm`, `reserved`, special `sensor_id=0xFE`, a dedicated flag bit); (b) `PKT_AIR_QUALITY = 0x04` is already reserved in `protocol.h` for a future **genuinely larger, 22-byte packet type** when more fields are needed than repurposing allows.

GPS position (lat, lon, speed, course, and ideally altitude) needs roughly: lat (int32, 1e-7 deg ≈ 1.1cm resolution) + lon (int32) + speed (uint16, cm/s or 0.1kn) + course (uint16, 0.1°) ≈ 12 bytes minimum, more with altitude/HDOP — this does not fit in the 16-byte struct's spare fields the way ANI-01's two scalars did.

**When to use:** For the ESP-NOW mesh channel (CH3) specifically. ESP-NOW's actual on-air payload limit is ~250 bytes (classic) — the 16-byte struct is an Aguada convention, not a hardware ceiling — so a **new, larger packet type is the correct fit here**, following the `PKT_AIR_QUALITY` precedent rather than trying to multi-packet-split lat/lon across several 16-byte frames (which adds reassembly/ordering complexity for no benefit, since the payload easily fits in one larger frame).

**Trade-offs:** A new packet type (e.g. `PKT_GPS_POS = 0x05`, ~24-28 bytes with its own CRC) is simplest and matches the codebase's own established extension pattern. It must still carry `node_id`, `seq`, `rssi`/`vbat`, `flags`, and CRC like every other packet so the gateway's existing JSON-serialization path and `bridge.py`'s handler-dispatch-by-`type` pattern extend naturally, rather than needing a parallel side-channel.

**Example:**
```c
// New in firmware/shared/protocol.h — separate struct, own type byte, still CRC-checked
#define PKT_GPS_POS 0x05
typedef struct __attribute__((packed)) {
    uint8_t  version;      // 0x03
    uint8_t  type;         // PKT_GPS_POS
    uint16_t node_id;
    uint8_t  sensor_id;    // 0 (n/a for GPS)
    uint8_t  ttl;
    uint16_t seq;
    int32_t  lat_e7;       // degrees * 1e7
    int32_t  lon_e7;
    uint16_t speed_cms;    // cm/s
    uint16_t course_d10;   // degrees * 10
    int8_t   rssi;
    int8_t   vbat;
    uint8_t  flags;
    uint16_t crc;          // CRC-16/CCITT over preceding bytes
} espnow_gps_packet_t;     // ~26 bytes, well under ESP-NOW's ~250B payload ceiling
```

### Pattern 4: HA `device_tracker` via MQTT Discovery, `json_attributes_topic`-driven

**What:** Home Assistant's MQTT `device_tracker` integration can derive location purely from `json_attributes_topic`: if the JSON payload contains `latitude`, `longitude`, and (optionally) `gps_accuracy`, HA plots it on the map without needing a separate `home`/`not_home` `state_topic`. `source_type` should be `"gps"`. This is the natural fit for a GPS tracker (as opposed to the `home_assistant_person_location`-style zone/presence trackers).

**When to use:** For the map integration requirement in PROJECT.md. Fits the same Discovery-on-HELLO pattern Aguada already uses for reservoir sensors — the tracker publishes its Discovery config once (on boot/first-contact), then just keeps updating the attributes topic.

**Trade-offs:** If a `state_topic` is ever added later (e.g. for a derived "moving"/"parked" state), remember it *overrides* the GPS-derived location until a `payload_reset` is sent — easy to accidentally break map updates by adding a state topic carelessly. For this tracker, skip `state_topic` entirely and drive everything off `json_attributes_topic`.

**Example:**
```json
// homeassistant/device_tracker/aguada_gpstracker_<node_id>/config
{
  "name": "Tracker",
  "json_attributes_topic": "aguada/gps/<node_id>/attributes",
  "source_type": "gps",
  "unique_id": "aguada_gps_<node_id>",
  "device": {"name": "GPS Tracker", "identifiers": ["aguada_gps_<node_id>"]}
}
// aguada/gps/<node_id>/attributes
{"latitude": -25.4284, "longitude": -49.2733, "gps_accuracy": 5, "speed_kmh": 42, "altitude_m": 12, "ts": 1741780800}
```

### Pattern 5: Segment-file + cursor logging instead of true in-place ring buffer

**What:** A literal overwrite-oldest ring buffer implemented as a single growing/shrinking file on LittleFS performs badly (rewriting the head forces rewriting everything after it), and per-record metadata overhead can dominate for many tiny appends. The better-fitting pattern for track logging is: append fixed-size fix records sequentially into a file/segment; roll to a new segment at a size/count/time threshold; keep a tiny separate cursor (last fully-uploaded segment+offset) in NVS or its own small file; delete/reclaim only fully-synced segments once acked.

**When to use:** For `Track Logger` in this design — matches "flash ring-buffer with upload cursor" from the milestone brief, but implemented as append+cursor+segment-rotation rather than literal circular overwrite, which is both simpler and flash-friendlier.

**Trade-offs:** Slightly more bookkeeping (segment index) than a naive ring buffer, but avoids LittleFS's worst-case rewrite behavior and makes "resume upload after reboot" trivial (cursor = segment id + byte offset).

**Example:**
```cpp
struct FixRecord { uint32_t ts; int32_t lat_e7, lon_e7; uint16_t speed_cms, course_d10; uint16_t alt_m; };
// /track/seg_00042.bin  — append-only, fixed-size records
// /track/cursor.bin     — {uploaded_seg, uploaded_offset}
```

## Data Flow

### Fix capture → durable store → opportunistic delivery

```
NEO-6M/M8N UART → NMEA parse → Fix{lat,lon,speed,course,alt,ts,fix_quality}
    ↓ (adaptive cadence: 5-10s moving / near-idle when stationary+no movement)
Track Logger.append(Fix)  → segment file on flash (always happens, regardless of connectivity)
    ↓
Connectivity State Machine polls: hotspot in range? known WiFi in range? ESP-NOW peer heard? none?
    ├─ CH1 Hotspot   → WiFi STA connect → MQTT publish latest Fix → HA device_tracker updates live
    ├─ CH2 Known WiFi→ WiFi STA connect → walk unsynced records from cursor → bulk MQTT/HTTP →
    │                    on ack, advance cursor, delete fully-synced segments
    ├─ CH3 ESP-NOW   → send PKT_GPS_POS to nearest Aguada node/gateway → gateway relays like any
    │                    other packet → gateway USB JSON → bridge.py new GPS handler
    └─ none          → deep sleep, retry on timer/motion interrupt
```

### bridge.py ingestion (three inbound paths converge on one state)

```
[MQTT: aguada/gps/<id>/live]        [MQTT: aguada/gps/<id>/bulk]      [USB JSON: type=GPS_POS from gateway]
            │                                  │                                  │
            └──────────────────┬───────────────┴──────────────────────────────────┘
                                ▼
                 bridge.py GPS ingest module (dedup by seq/ts, out-of-order tolerant)
                                ▼
              ┌─────────────────────────────┬─────────────────────────┐
              ▼                             ▼                         ▼
   MQTT Discovery / device_tracker    Append to server-side track   (optional) HA
   json_attributes_topic (live map)   history store (sqlite/csv)     "last_seen" sensor
                                                │
                                                ▼
                                      gpx_export.py → .gpx on demand
```

### Key Data Flows

1. **Never-lose-a-point:** every fix is written to flash before any network attempt is made, on all three channels; the network layer only ever *reads* from durable storage (for CH2/CH3 bulk/relay) or opportunistically forwards the just-logged fix (CH1 live). Losing power or connectivity at any point loses at most the in-flight, not-yet-acked upload, never the record itself.
2. **Convergent server ingestion:** three physically different transports (MQTT live, MQTT/HTTP bulk, ESP-NOW-via-gateway-USB-JSON) all funnel into one `bridge.py` handler so HA and the GPX export only ever see one unified, deduplicated track — the node-side channel diversity is invisible past the bridge.
3. **GPX stays server-side:** raw fixes are the only thing that crosses the wire/mesh; format conversion, gap-filling across partial uploads, and any smoothing happen in `tools/gpx_export.py`, consistent with "nodes transmit only raw values, math done at the bridge."

## Scaling Considerations

| Scale | Architecture Adjustments |
|-------|--------------------------|
| 1 prototype (this milestone) | Everything above as designed: single tracker, single bridge.py process, flash log measured in weeks of track data |
| Small fleet (a few trackers) | `node_id`-keyed dedup/store already generalizes (same pattern as `reservoirs.yaml` keyed by node_id); bridge.py GPS module needs per-tracker state (cursor tracking is server-side too, to detect gaps) but no architecture change |
| Larger fleet / provisioning at scale | Explicitly out of scope per PROJECT.md; would eventually need per-device auth/allowlisting, a real time-series store instead of sqlite/csv, and possibly moving GPS ingestion off the bridge process — deferred |

### Scaling Priorities

1. **First likely friction:** flash storage growth vs. sync frequency — if the vehicle goes long stretches without any of the 3 channels, segment count grows; the cursor+segment-rotation design (Pattern 5) already handles this gracefully, but pick a sane max-retained-segments cap so a permanently-offline tracker degrades (drops oldest unsynced data) rather than filling flash and crashing.
2. **Second:** WiFi scan/connect latency budget vs. battery — every CH1/CH2 attempt costs real time+current (scan + associate + possibly fail); the state machine should back off (exponential or fixed longer interval) after repeated failed scans, especially on battery-only power (ignition off), rather than retrying aggressively.

## Anti-Patterns

### Anti-Pattern 1: Assuming WiFi and ESP-NOW can run concurrently on arbitrary channels

**What people do:** Design the tracker as if it can "stay joined to the mesh via ESP-NOW while also being connected to a phone hotspot for live MQTT," treating all 3 channels as simultaneously available.
**Why it's wrong:** ESP32 has one radio and one home channel; per ESP-IDF's own docs, in STA/AP coexistence the connected AP's channel takes priority and the radio migrates to it, breaking ESP-NOW peers pinned to a different fixed channel (Aguada mesh is fixed on channel 1, and a phone hotspot's channel is not controllable/predictable). Discovering this after building an "always dual-connected" prototype is the highest-risk rewrite in this milestone.
**Do this instead:** Build the connectivity layer as an explicit, tested mutually-exclusive state machine from day one (Pattern 1); accept that only one channel is active at a time.

### Anti-Pattern 2: Doing GPX conversion or reservoir-style math on the node

**What people do:** Try to have the node itself produce a ready-to-use GPX file or do trajectory smoothing/route matching on-device, mirroring "do everything locally" habits from simpler single-purpose GPS logger projects.
**Why it's wrong:** Contradicts the Aguada system's own established principle (nodes send raw values, `bridge.py` does all derived math) and adds firmware complexity/flash/RAM pressure to a battery-constrained device for something the server can do trivially and can also do *better*, since only the server ever sees the fully reassembled track after multi-channel opportunistic sync.
**Do this instead:** Node stores and forwards raw fixes only; `tools/gpx_export.py` (new, server-side) builds GPX from the ingested history in `bridge.py`.

### Anti-Pattern 3: True in-place ring-buffer file for track storage

**What people do:** Implement the "flash ring-buffer" requirement literally as a fixed-size file with wraparound overwrite at the byte level.
**Why it's wrong:** LittleFS (and flash filesystems generally) handle head-of-file rewrites and many tiny in-place writes poorly — costly rewrites and metadata overhead that can exceed the data itself.
**Do this instead:** Append-only segment files + a small separate cursor + segment rotation/deletion once synced (Pattern 5) — same end-user behavior (bounded storage, never lose unsynced data) without the filesystem-hostile access pattern.

### Anti-Pattern 4: Reusing the 16-byte packet's spare fields to smuggle lat/lon

**What people do:** Try to cram latitude/longitude into `distance_cm`/`reserved`/`vbat` the way ANI-01 did for eCO2/AQI/temp/humidity, splitting across many packets to fit the budget.
**Why it's wrong:** eCO2/AQI/temp/humidity are each small scalars that fit a `uint16`/`int8`; lat/lon need ~8 bytes alone at usable precision, plus speed/course — reusing 16-byte frames would need 3+ packets per fix with fragile reassembly, for a value ESP-NOW can carry in one frame anyway (payload ceiling ~250 bytes).
**Do this instead:** Define a new, appropriately-sized packet type (`PKT_GPS_POS`), exactly as `PKT_AIR_QUALITY` is already reserved in `protocol.h` for this same class of problem (Pattern 3).

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| Phone hotspot (CH1) | ESP32 as WiFi STA, SSID/password stored in NVS, connects opportunistically when detected | Channel is whatever the phone chooses — cannot coexist with fixed-channel ESP-NOW; must fully exit ESP-NOW mode first |
| Known home/base WiFi (CH2) | Same WiFi STA mechanism as CH1, different SSID list, triggers bulk upload instead of live single-fix publish | Reuse the same `uplink_mqtt` module, different trigger/behavior |
| Aguada ESP-NOW mesh (CH3) | New `PKT_GPS_POS` type on existing channel-1 mesh, relayed by existing nodes/gateway exactly like `PKT_SENSOR`/`PKT_HEARTBEAT` | No changes needed to existing node/gateway relay logic beyond recognizing the new type and passing it through to USB JSON |
| MQTT broker / Home Assistant | Same broker already used by `bridge.py`; new topics under an `aguada/gps/...` namespace, new Discovery config for `device_tracker` | Keep GPS topics namespaced separately from `aguada/{node_id}/{sensor_id}/state` reservoir topics to avoid collision with `reservoirs.yaml` node_id keying |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| GPS Reader ↔ Track Logger | in-memory `Fix` struct, function call | No network/serialization involved on this boundary |
| Track Logger ↔ Uplink modules (mqtt/espnow) | Uplink modules only ever *read* from Track Logger via cursor, never bypass it | Enforces store-first invariant; no direct GPS-reader-to-network path should exist |
| Connectivity State Machine ↔ Uplink modules | State machine owns radio lifecycle (init/deinit/mode switch); uplink modules only run when the matching mode is active | Prevents uplink code from silently assuming a stale WiFi/ESP-NOW state |
| Gateway ↔ bridge.py | Existing USB JSON serial link, just gains one more `type` value in the JSON schema | No new physical integration point — reuses existing gateway↔bridge boundary |
| bridge.py GPS module ↔ existing bridge.py reservoir logic | Same process, separate handler/module, separate MQTT topic namespace | Keep as a clearly separated module so it can be extracted to its own process later if it needs to scale beyond one prototype |

## Sources

- `firmware/shared/protocol.h` (read directly) — current 16-byte packet, packet types, `PKT_AIR_QUALITY` reservation — HIGH confidence (primary source)
- `AGUADA_SYSTEM_DOC.md` §7-10, §17 (read directly) — protocol, gateway JSON, bridge.py MQTT topics/Discovery, ANI-01 multi-packet extension precedent — HIGH confidence (primary source)
- `.planning/PROJECT.md` (read directly) — milestone requirements, constraints, out-of-scope — HIGH confidence (primary source)
- ESP-IDF official docs, via Context7 (`/espressif/esp-idf`, WiFi driver overview + `esp_now.h`) — home-channel / STA-AP coexistence / `esp_now_peer_info_t.channel` semantics — MEDIUM confidence (official docs, cross-checked against known ESP32 radio architecture)
- Web search: Home Assistant `device_tracker.mqtt` docs (home-assistant.io) — `json_attributes_topic` GPS behavior — LOW-sourced tier per this session's classifier, but corroborated by official HA docs URL surfaced in results
- Web search: general offline-first/store-and-forward IoT tracker architecture (industry blog posts) — LOW confidence, used only for pattern-naming/validation, not for any Aguada-specific decision
- Web search: LittleFS/ESP32 ring-buffer/wear-leveling discussion (ESP32 forum, LittleFS GitHub issue) — LOW confidence, corroborates well-known flash-filesystem behavior

---
*Architecture research for: GPS tracker integration into Aguada telemetry system*
*Researched: 2026-08-01*
