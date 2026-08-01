# Pitfalls Research

**Domain:** ESP32 GPS vehicle/boat tracker (no GSM) — offline flash logging + opportunistic WiFi/ESP-NOW sync, integrated into existing Aguada ESP-NOW mesh + bridge.py + Home Assistant system
**Researched:** 2026-08-01
**Confidence:** MEDIUM-HIGH (grounded in ESP-IDF docs, u-blox docs, ESP32 forum consensus, and HA integration behavior; project-specific integration risk is architecture judgment, not benchmarked)

## Critical Pitfalls

### Pitfall 1: ESP-NOW and WiFi-STA fight over the radio channel

**What goes wrong:**
ESP-NOW and WiFi station mode share the same 2.4GHz radio and **must be on the same channel**. Once the ESP32 associates to an AP (the phone hotspot or a known home/gateway WiFi), the channel is fixed by that AP and cannot be changed by the app. If the Aguada mesh operates on a different channel than the AP the tracker just joined, ESP-NOW to the Aguada gateway/nodes silently stops working — packets go nowhere, with no error reported by `esp_now_send()` (it still returns `ESP_OK`, only the send callback later reports failure, and only if a peer ACK was expected).

**Why it happens:**
Developers test ESP-NOW-only and WiFi-only in isolation, both work, then wire them together and assume they compose. The tracker is uniquely exposed to this because it has **three channels that all claim the radio**: phone hotspot (unknown/uncontrolled channel), known WiFi (fixed channel, may differ from Aguada mesh channel 1), and Aguada ESP-NOW mesh (fixed at channel 1 per `AGUADA_SYSTEM_DOC.md`). Also, `esp_wifi_set_channel()` fails silently or is ignored while STA is connecting/scanning/connected — the failure mode is "nothing happens," not an exception.

**How to avoid:**
- Never try to run ESP-NOW and STA-to-arbitrary-AP simultaneously on the same radio state. Instead, state-machine the tracker: **mutually exclusive modes** — (a) ESP-NOW-only (channel 1, mesh proximity), (b) WiFi-STA-only (hotspot/known AP, arbitrary channel), never both "live" at once.
- Before entering WiFi-STA mode, first do a passive scan (or just try) for the Aguada mesh in range; if found, prefer ESP-NOW delivery (cheap, low latency, no channel conflict) and skip WiFi for that cycle.
- If simultaneous operation is truly required later, use `WIFI_AP_STA` mode and force both mesh and AP onto the same channel (only works if you control the AP, i.e. NOT for phone hotspots — hotspot channel is not controllable, so ESP-NOW to Aguada is fundamentally sacrificed while hotspot-connected).
- Log the active WiFi channel and whether ESP-NOW peer add/send succeeded distinctly, so this is debuggable in the field.

**Warning signs:**
- ESP-NOW sends "succeed" (callback fires OK) but the gateway never logs receiving them while the tracker is WiFi-connected.
- Works on the bench (single channel, same room) but fails once tested in the actual vehicle/mesh environment.

**Phase to address:**
Radio/connectivity architecture phase (channel arbitration + mode state machine), before the 3-channel sync logic is built on top of it.

---

### Pitfall 2: Ring buffer / log file design causes flash wear-out or data loss during heavy logging

**What goes wrong:**
Naive approaches either (a) append every point as a new tiny file (LittleFS/SPIFFS overhead per file, directory metadata churn) or (b) rewrite one big track file per write (`fopen` append should be fine, but many implementations mistakenly do read-modify-write of the whole file, e.g., to prepend headers or re-serialize JSON arrays). At 1 write per 5–10 s continuous, that's ~500–1700 writes/hour; over months of daily driving this becomes tens of thousands of flash writes to the same erase blocks if wear-leveling headroom is small. SPIFFS specifically is known to slow down and even corrupt above ~70% of partition fill and after large numbers of file create/delete cycles, and is unmaintained upstream — a real risk if the log directory pattern is "one file per trip" with frequent create/delete.

**Why it happens:**
Reference project (ESP32_IMU_BARO_GPS_VARO style) logs to external SPI flash with wear leveling; adapting that pattern to internal ESP32 flash without matching partition size to actual write volume, or choosing SPIFFS out of familiarity, both bite later. Developers also frequently store trajectory as JSON/array-in-file (edit-heavy) instead of an append-only binary record log (write-only, one write = one `write()` of a fixed-size struct at EOF).

**How to avoid:**
- Use **LittleFS** (not SPIFFS — deprecated, has documented corruption issues under GC pressure) on internal flash, or an external SPI flash chip with a proper wear-leveling FS if trajectory retention needs to be large (weeks of continuous logging).
- Log as an **append-only binary ring buffer of fixed-size records** (fixed struct: timestamp, lat, lon, alt, speed, course, fix quality) — never rewrite/reformat the whole file. Roll to a new file at a size threshold (e.g. 1 MB) and delete/archive oldest once flash-consumed hits a cap, rather than doing in-place compaction.
- Size the write cadence to the wear budget: NOR flash sectors are typically rated ~100k erase cycles; back-of-envelope the number of erase-block reuses per year at your write rate and partition size before committing to "5–10 s in motion" as the default — this is a capacity-planning task, not a guess.
- Batch writes: buffer N points in RAM and flush every 30–60 s (not every point) if power budget allows, trading a small backfill-on-crash risk for 6–12x fewer flash writes.

**Warning signs:**
- Increasing write latency to the log file over weeks of use (classic SPIFFS GC symptom).
- Corrupted/truncated track files appearing only after extended field use, not in short bench tests.

**Phase to address:**
Flash logging/storage phase — before adaptive cadence or sync logic, since the storage format is a foundational contract that the exporter and store-and-forward cursor both depend on.

---

### Pitfall 3: Store-and-forward cursor produces duplicate or lost points on crash/reconnect

**What goes wrong:**
The "upload cursor" (pointer to last-synced record) is often kept only in RAM, or is updated *before* the MQTT publish is confirmed rather than after. Result: a power loss or WiFi drop mid-batch either (a) re-sends already-delivered points (duplicates in HA history / wasted bandwidth) or (b) skips points that were buffered but never actually delivered (silent gaps in the trajectory — exactly the "no point ever lost" core value this milestone promises).

**Why it happens:**
It's tempting to advance the cursor as soon as points are handed to the MQTT client library, since `publish()` returns immediately — but that's before the broker (or even the TCP socket) has actually accepted the message, especially over a flaky phone hotspot. Store-and-forward over an unreliable, intermittent link is inherently a two-phase problem (send, then confirm) that simple linear "read next N, send, advance pointer" code skips.

**How to avoid:**
- Persist the cursor to flash (not just RAM), and only advance it after receiving MQTT PUBACK (QoS 1) for the batch, or after an explicit application-level ACK from `bridge.py`/the HA-side consumer.
- Give every trajectory point a stable **monotonic id** (e.g., record index within the log, not just a timestamp — timestamps can collide before GPS fix or repeat across reboots if RTC isn't backed). Use that id for idempotency: the receiving side (bridge.py or a small ingestion script) should dedupe on id, so a resend-after-crash is harmless rather than a corrupt duplicate trace.
- Prefer QoS 1 + dedupe over QoS 2 — QoS 2 is expensive and still doesn't save you if the id/dedupe layer is missing.
- Test explicitly: pull the WiFi/power mid-batch-upload repeatedly and verify the resulting track (post-dedupe) matches the flash log exactly, no gaps, no dupes.

**Warning signs:**
- Track history in HA/GPX has repeated points at the same lat/lon with different timestamps clustered around known WiFi drop events.
- Gaps in the trajectory that don't correspond to actual "parked/stationary" periods.

**Phase to address:**
Store-and-forward sync phase — cursor persistence + idempotent ingestion must be designed together, not bolted on after "happy path" upload works.

---

### Pitfall 4: NMEA/UBX parsing without fix-quality filtering feeds garbage into the log

**What goes wrong:**
NEO-6M/M8N will output NMEA sentences (or UBX messages) continuously, including when it has **no fix or a degraded fix** (fix type 0/1, low satellite count, high HDOP). Naive parsers grab lat/lon out of `$GPGGA`/`$GPRMC` as soon as fields are non-empty, without checking the fix-quality/status field, and log implausible jumps (e.g., last-known-position artifacts, 0,0 "null island" coordinates, or huge single-sample jumps from a momentary multipath glitch near buildings/marine environments). This is worse on a boat (open water reflections, mast/superstructure multipath) and in urban driving (building canyon multipath) — exactly the two use cases named for this tracker.

**Why it happens:**
Tutorials (and libraries like TinyGPS++) make `lat()`/`lng()` trivially accessible without forcing the caller to check `location.isValid()` / fix quality / HDOP first. It "looks like it works" on the bench near a window with a clean sky view, then produces spiky nonsense in the field.

**How to avoid:**
- Only accept a fix when: fix type ≥ 2D (NMEA `GGA` field 6 fix quality ≥ 1, ideally ≥ 2 for DGPS, or UBX `NAV-PVT` `fixType` ≥ 2), satellites in use ≥ 4 (ideally ≥ 6), and HDOP below a threshold (e.g. < 3–5 for logging, stricter if precision matters).
- Reject and discard (don't log) points during the first few seconds after a fix is acquired — the very first fix after cold/warm start is often the least accurate; consider requiring N consecutive valid fixes before trusting position for cadence/movement decisions.
- Add a simple outlier gate on top of fix-quality filtering (mirrors the existing Aguada water-level 3-layer filter philosophy): reject a new point if implied speed vs. the last accepted point exceeds a sane max (e.g. >200 km/h for a vehicle, tunable) — catches multipath jumps that still report a "valid" fix.
- Verify UART settings match: NEO-6M/M8N default to 9600 baud NMEA — mis-set baud silently yields garbled/no sentences, easily confused with "no fix" in logs if you don't distinguish "no valid NMEA parsed" from "valid NMEA, no fix" from "valid fix."

**Warning signs:**
- Track logs contain teleport-like jumps or points at (0,0) / mid-ocean when device is stationary on land.
- GPX export shows a "spider web" of spurious lines when overlaid on a real road/route.

**Phase to address:**
GPS acquisition/parsing phase — fix-quality gating must exist before adaptive cadence and before any log write, since every downstream feature (cadence, HA display, GPX) trusts whatever the parser accepted.

---

### Pitfall 5: Backup battery / hot-start omitted, so every power cycle pays full cold-start TTFF

**What goes wrong:**
Without a VBAT backup (coin cell or supercapacitor) on the NEO-6M/M8N, every time the module loses main power (deep sleep power-gated, or vehicle ignition off long enough that the Li-ion protection/regulator drops out) it loses ephemeris/almanac and RTC, forcing a cold start (~30–40 s TTFF) on next wake instead of a hot start (~1–3 s). For a design whose whole point is deep-sleep-heavy operation on Li-ion backup, this directly multiplies power draw (radio+MCU awake 10-40x longer than needed per wake cycle) and delays the "first useful fix" after every stop-start cycle — undermining both the energy budget and the "no point lost" value (you lose the entire cold-start window every time).

**Why it happens:**
The VBAT pin is easy to skip on a breadboard/prototype (bench power is always on), and its necessity only becomes obvious once the device actually experiences real power cycling in the field (ignition off, deep sleep). Also, cutting main power to the GPS module (e.g., via a MOSFET load switch to save current during deep sleep) — a very reasonable-looking power optimization — completely defeats hot-start if VBAT isn't wired to a separate always-on source.

**How to avoid:**
- Wire VBAT to a small dedicated backup source (CR2032 or supercap) that stays powered independent of the module's main VCC, so RTC + BBR (battery-backed RAM: last position, almanac, clock) survive main power loss. Backup current draw is ~10-15 µA — trivial vs. the TTFF savings.
- If deep-sleeping the whole board rather than power-gating the GPS module specifically, decide explicitly: either (a) keep GPS module VCC alive through MCU deep sleep (module has its own low-power modes) — costs more idle current but preserves hot start — or (b) power-gate GPS VCC but guarantee VBAT stays fed, and budget for hot-start (~1-3s) not cold-start in wake-cycle timing.
- Measure actual TTFF in the deployed enclosure/antenna position, not just on the bench with a clear sky view — obstructed antenna extends all start times.

**Warning signs:**
- First fix after every wake takes 20-40 s consistently (cold-start signature) instead of settling to 1-3 s after the first successful fix of a session.
- Power budget calculations assume hot-start timing but field battery life is far shorter than modeled.

**Phase to address:**
Power architecture phase (deep sleep + Li-ion budget) — must be co-designed with the GPS acquisition phase, since the sleep strategy determines whether hot-start is even achievable.

---

### Pitfall 6: 12V automotive supply noise/transients kill the regulator or desensitize the GPS receiver

**What goes wrong:**
Automotive 12V rails have load-dump transients (up to 40-80V spikes when alternator load is suddenly removed, e.g. battery disconnect while charging), plus cranking dips (down to ~4-6V during engine start) and general switching/ignition noise. A buck converter feeding the ESP32+GPS from raw 12V without transient protection can be damaged by load-dump, brown out during cranking (causing mid-log resets — silent gaps or corrupted records right at record-boundaries), or — subtler — inject switching-frequency ripple/noise that couples into the GPS antenna/RF front-end and degrades sensitivity (more multipath-like errors, longer TTFF, intermittent fix loss), especially if GPS antenna/module is placed near the buck converter or shares a noisy ground plane.

**Why it happens:**
"12V to 5V to 3.3V, done" is the mental model from hobby electronics with a bench supply; automotive-grade supply design (transient suppression, brownout-tolerant sequencing, RF-quiet regulator placement/layout) is a different discipline that's easy to skip when the prototype "just works" plugged into a lab supply or even a cigarette-lighter USB adapter (which already did this filtering for you).

**How to avoid:**
- Use an automotive-rated buck converter (rated for load-dump per ISO 7637-2, or add a TVS diode + reverse-polarity protection + input fuse ahead of a standard buck) rather than a generic hobby buck module.
- Physically separate the GPS antenna/module from the buck converter and route the GPS module's power through additional LC/ferrite filtering; keep GPS analog ground short and separate from high-current switching return paths (mirrors the "run GPS on its own ground trace, avoid sharing with motor drivers" guidance that also applies to buck converter switching noise).
- Brown-out test explicitly: simulate a cranking dip (temporary sag to ~6V) and confirm the ESP32 either rides through via input capacitance/buck holdup or does a clean recorded reboot (not a silent flash-write corruption) — this is a "looks done but isn't" item, easy to miss because bench testing never dips the input rail.
- Consider deriving MCU power from the Li-ion backup during transient events (supercapacitor or battery holds the 3.3V rail steady) rather than solely from the 12V-derived buck output, so momentary automotive transients don't propagate to logging.

**Warning signs:**
- Unexplained resets or log corruption correlating with engine start/stop, not with software crashes.
- GPS fix quality noticeably worse when engine running vs. engine off (RF noise coupling signature).

**Phase to address:**
Power architecture / hardware integration phase, ideally validated with an actual vehicle (not just bench 12V supply) before considering hardware "done."

---

### Pitfall 7: Adaptive cadence logic falsely detects movement from GPS jitter while stationary

**What goes wrong:**
"Increase logging rate when moving, slow down when stationary" sounds simple, but consumer GPS position jitter while genuinely stationary is commonly several meters (worse with poor sky view — parking garage, marina berth under a hull/dock structure). A naive "distance since last point > threshold → moving" check flags this jitter as movement, causing the tracker to never enter the low-power/low-write "stationary" state: continuous full-rate logging (defeating both the flash-wear and battery-budget goals) plus a track full of tiny random spurs when parked, polluting the GPX export.

**Why it happens:**
Developers test cadence logic with real driving data (movement is obviously real) but rarely test the stationary case with the actual receiver in the actual planned mounting location — jitter characteristics depend heavily on antenna placement/sky view, so bench testing outdoors with clear view looks perfectly stable while a windshield-mounted or below-deck unit in the field is not.

**How to avoid:**
- Use **speed from the GPS solution** (NMEA `RMC`/`VTG` ground speed, or UBX `NAV-PVT` speed with its accuracy estimate) as the primary movement signal, not raw position delta — receiver speed estimates are typically far more stable than differencing noisy positions, especially when moving speed is near zero.
- Require a **debounce window**: N consecutive samples above a speed threshold (e.g., >1-2 m/s for a few seconds) before switching to "moving" cadence, and similarly require sustained low-speed samples before switching back to "stationary," to avoid mode-flapping.
- Combine with fix-quality gating (Pitfall 4) — jitter is worst exactly when HDOP is poor, so a coupled "moving" decision that ignores HDOP will fire most often in the worst-sky-view spots (parking garages, marinas).
- Field-test the stationary case in the actual planned mounting location for multiple hours, not just outdoors on a bench, before considering cadence logic validated.

**Warning signs:**
- GPX track shows a tight tangle of short random segments at parking spots instead of a single stationary point/marker.
- Flash write rate and battery drain don't drop when the vehicle/boat is actually parked for extended periods.

**Phase to address:**
Adaptive cadence phase — should be built and tested after fix-quality filtering exists, using speed-based (not distance-based) movement detection, and validated in the real mounting environment before shipping.

---

### Pitfall 8: Home Assistant `device_tracker`/MQTT history cannot be backdated, breaking the "sync later" model

**What goes wrong:**
This tracker's core value proposition is offline logging + later batch sync ("despeja histórico completo em lote" when WiFi appears). But Home Assistant's state machine (and its recorder/history) timestamps a state change at the moment HA **receives** the MQTT message, not at any timestamp embedded in the payload. If the store-and-forward batch replays hours or days of historical points through `device_tracker`/`sensor` MQTT topics, HA's history/map will show them all clustered at the upload moment (or animate through them in seconds), not at their true recorded times — the graph/map is misleading, and there is no supported way to make HA's built-in recorder/history retroactively reflect the real GPS timestamps for a live-updating entity.

**Why it happens:**
This is a structural HA limitation (state machine = "current state now," not a time-series database with arbitrary insert timestamps), not a bug — but it's routinely discovered only after the batch-upload path is built, because the "live" channel (hotspot MQTT) works exactly as expected and the developer assumes store-and-forward will behave the same way.

**How to avoid:**
- Don't try to make backfilled history appear correctly *inside* HA's native recorder/history for `device_tracker`. Instead: (a) use the live channel (hotspot, ESP-NOW-to-gateway) for what HA shows as "current state / recent history," and (b) treat the flash-logged full trajectory as a **separate artifact** — export it as GPX (already planned) or ingest it into a purpose-built store (e.g., a small InfluxDB/SQLite table in the bridge, or directly write historical rows via HA's long-term-statistics/recorder DB import tooling if precise backdated graphing in HA is a hard requirement) rather than replaying it through live MQTT state topics.
- If some in-HA visualization of the historical route is wanted, consider a dedicated custom card/map (e.g., rendering the GPX/track table directly, not via `device_tracker` state history) rather than fighting the recorder.
- Set expectations in the roadmap: "HA shows live position accurately; full trajectory analysis happens via GPX export," rather than promising perfect backdated HA history — this avoids late discovery of a hard platform limitation.

**Warning signs:**
- After a WiFi reconnect/batch sync, the HA map/history shows all backlogged points arriving "just now" or animates through the whole offline period in a few seconds.
- Long-term statistics/graphs for the tracker show a flat gap for the offline period, immediately followed by a spike, instead of a smooth trace.

**Phase to address:**
HA integration phase — the batch-sync/HA-integration design should explicitly separate "live state" (HA-native) from "historical trajectory" (GPX/external store) from the start, not as a workaround discovered late.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|-----------------|------------------|
| Log trajectory as JSON array in a single growing file | Fast to implement, human-readable for debugging | Read-modify-write on every append = flash wear + O(n) write cost as file grows; risk of corruption on power loss mid-rewrite | Never for production; OK only for a throwaway first bench prototype |
| Skip fix-quality/HDOP filtering, log every NMEA fix | Simpler parser, fewer edge cases to handle early | Garbage points pollute GPX/HA map, corrupt adaptive-cadence movement detection | Never past the first bench smoke-test |
| Advance store-and-forward cursor immediately on `publish()` call (no ACK wait) | Simpler code, "looks synced" instantly in demos | Silent data loss or duplication on any mid-batch disconnect — directly violates the "no point ever lost" core value | Never — this is the one shortcut this project cannot afford |
| Hardcode ESP-NOW channel = hotspot's channel assumption | Avoids building a channel-arbitration state machine | Breaks mesh delivery the moment hotspot channel differs (it will, hotspots pick channels dynamically) | Never; acceptable only as an explicit "ESP-NOW disabled while WiFi-STA active" documented limitation for MVP |
| No VBAT backup battery, cold-start every wake | Saves a component/BOM line, simpler board | 10-40x TTFF penalty every wake cycle, larger battery drain, worse UX | Only acceptable if device is never deep-sleep/power-cycled (i.e., contradicts this project's own power design) |
| Skip automotive-grade power protection, use hobby buck converter | Cheaper, faster to source | Field failures from load-dump/cranking transients, GPS RF desensitization; hard to diagnose after the fact | Only for a bench/indoor prototype never installed in an actual vehicle |

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|-----------------|-------------------|
| Phone hotspot MQTT/internet | Assuming the hotspot always has internet reachability and a public/forwarded broker path; ignoring that carrier-grade NAT means the phone itself usually can't be inbound-reached | Tracker connects **outbound** to a broker with a public/known address (small VPS, or existing bridge.py host if it has a reachable public IP/DDNS) over the hotspot's outbound internet; never assume port-forwarding into the phone or into a home network behind CGNAT is available |
| MQTT broker over the internet (vs. existing LAN-only bridge.py setup) | Reusing the LAN MQTT broker config (no TLS, weak/no auth) as-is for an internet-facing hotspot connection | Require TLS (mqtts, 8883) and per-device credentials for the internet-facing path; do not reuse the trusted-LAN assumption baked into the existing Aguada bridge.py MQTT setup |
| ESP-NOW packet size (existing 16-byte v3 protocol) | Trying to cram lat/lon/speed/course into the existing 16-byte struct via lossy encoding, breaking compatibility with existing sensor nodes/gateway parsing | Add a new packet `type` (e.g. `0x30 GPS_POSITION`) with its own larger payload/multi-packet framing, explicitly versioned, so the gateway's existing 16-byte v3 parser path is untouched for existing node types |
| TinyGPS++/NMEA library defaults | Trusting `gps.location.lat()/lng()` without checking `gps.location.isValid()` and age of fix | Always gate on `isValid()`, `age()` (reject stale cached values), and fix-quality/HDOP as described in Pitfall 4 |
| bridge.py ingestion of GPS batch uploads | Extending the existing per-message MQTT handler (built for near-real-time sensor telemetry) to also silently accept large historical batches on the same topic/handler | Use a distinct topic/handler for batch/backfill uploads with idempotency (Pitfall 3) and rate-limiting, separate from the live single-point telemetry path |

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|-----------------|
| One growing JSON/text log file for the whole trip history | Increasing time to append a point; eventually a write takes seconds | Rotate to fixed-size binary record files (append-only), cap retained history size | Noticeable after a few thousand points (~a day of 5-10s cadence logging) |
| SPIFFS instead of LittleFS for the log partition | Progressive slowdown, occasional file corruption after weeks | Use LittleFS; monitor partition fill percentage and stay well under it | Documented degradation above ~70% partition fill |
| Distance-delta-based movement detection | Constant "moving" state even when parked (jitter), no power/flash savings | Speed-based + debounced detection (Pitfall 7) | Any time sky view is imperfect (garage, dock, foliage) |
| Uploading the entire backlog in one giant batch after long offline period | MQTT client buffer overrun, broker/QoS timeouts, huge RAM use assembling one big payload | Chunk uploads into bounded batches (e.g. a few hundred points), with per-chunk ACK/cursor advance | Breaks once offline period exceeds a day or two of continuous logging |

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Storing phone hotspot WiFi credentials and MQTT broker credentials in plaintext in firmware/NVS with no protection | Anyone who dumps flash (or a lost/stolen device) leaks the owner's hotspot password and broker access | Store credentials in NVS with flash encryption enabled if the threat model includes device loss/theft (vehicle theft is a real scenario here); at minimum don't hardcode in source |
| MQTT over the internet without TLS or with default/shared credentials | Anyone can inject fake position data or read the vehicle's live location/history | Require TLS + unique per-device credentials for the internet-facing broker connection (see Integration Gotchas) |
| No authentication/validation on the batch-backfill ingestion path in bridge.py | A malformed or spoofed batch could inject bogus trajectory history into HA/GPX exports | Validate payload structure, node id, and sequence/id ranges server-side before accepting backfill batches |
| GPS position broadcast unencrypted over ESP-NOW when near Aguada mesh | Real-time vehicle location leaked to anyone sniffing ESP-NOW frames near the mesh (low bar, but relevant for a vehicle-tracking use case) | Consider whether ESP-NOW payload encryption (ESP-NOW supports encrypted peers) is warranted given the location-privacy sensitivity of this data vs. plain sensor readings |

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|------------------|
| HA map shows the last known position indefinitely with no "stale" indication when tracker has been offline for hours/days | User believes the vehicle/boat is at a location it left long ago — worse than showing nothing | Publish an explicit `availability`/staleness attribute (e.g., last-update age) and surface it in the HA card/automation, not just raw lat/lon with no context |
| Batch sync dumps hours of history into HA at once, animating the map rapidly or spamming notifications/automations tied to `device_tracker` state changes | Confusing UX, possible automation storms (e.g., "arrived home" triggers firing repeatedly from replayed history) | Route backfill through a path that does not touch the live `device_tracker` entity's state history at all (see Pitfall 8); only ever update live state from the live channel |
| GPX export includes pre-fix-lock noise or garage/dock jitter clusters | Exported route looks messy/wrong when opened in Google Earth, undermining trust in the whole system | Apply the same fix-quality + outlier filtering to the GPX exporter as to the live path (single shared filtering function, not duplicated/divergent logic) |

## "Looks Done But Isn't" Checklist

- [ ] **GPS fix acquisition:** Often missing fix-quality/HDOP gating — verify by testing under a poor-sky-view mount (parking garage, under a hardtop) for an extended period and inspecting the raw vs. filtered log for jumps/null-island points.
- [ ] **Flash logging:** Often missing wear/capacity planning — verify by calculating erase-cycle budget at the chosen cadence and partition size, and by running a multi-day soak test watching write latency for the SPIFFS-style progressive-slowdown signature.
- [ ] **Store-and-forward:** Often missing crash-mid-batch handling — verify by repeatedly killing power/WiFi during an active batch upload and confirming zero duplicates and zero gaps in the resulting HA/GPX record after recovery.
- [ ] **ESP-NOW + WiFi coexistence:** Often "works on the bench" only because bench AP and mesh happen to share a channel — verify by testing with a phone hotspot on a different, realistic channel and confirming the tracker correctly falls back rather than silently dropping ESP-NOW traffic.
- [ ] **Power design:** Often missing VBAT backup and automotive-transient protection — verify by measuring actual cold vs. hot start times after real deep-sleep cycles, and by testing an engine crank event (voltage dip) for clean recovery, not a corrupted log.
- [ ] **Adaptive cadence:** Often validated only against driving data — verify by leaving the unit stationary in its real mount location for hours and confirming it drops to low-rate/low-power state and the resulting track has no phantom movement.
- [ ] **HA integration:** Often assumes backfilled history displays "correctly" — verify by triggering a real batch sync after an offline period and observing what the HA map/history actually shows (expect it to cluster at sync time; confirm the GPX/external path is the actual source of truth for historical route review).

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|----------------|-----------------|
| ESP-NOW silently broken while WiFi-connected | LOW | Add explicit mode logging/telemetry (channel, active mode) once discovered; retrofit the mutually-exclusive state machine — no data loss if flash logging is independent of ESP-NOW delivery |
| SPIFFS corruption/slowdown discovered in the field | MEDIUM | Migrate partition to LittleFS; if trajectory data on the corrupted partition is unrecoverable, fall back to whatever was already synced via live channels — underscores why the live channel + flash log should be treated as independently valuable, not sequential dependencies |
| Duplicate/missing points discovered in already-synced HA/GPX history | MEDIUM | Add monotonic-id + dedupe layer retroactively server-side (bridge.py); historical gaps before the fix can't be recovered if the flash log itself already rotated past them — recovery cost rises the longer this goes undetected, hence early testing priority |
| No VBAT wired, discovered after board is fabricated | LOW-MEDIUM | Bodge-wire a CR2032 holder or supercap to the VBAT pin if pin is exposed on the module breakout; if using a bare module without an accessible VBAT pin, may require a board respin |
| Automotive power design fails in real vehicle (resets on crank) | MEDIUM-HIGH | Add TVS/input protection and input holdup capacitance; may require a small hardware revision (added components) rather than pure firmware fix, so field units may need retrofit |
| HA history discovered to not backdate correctly | LOW | Reframe UX (see Pitfall 8) and redirect historical review to GPX/external store — no data is lost since flash log remains the source of truth; this is a display/expectation fix, not a data-recovery problem |

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|-------------------|----------------|
| ESP-NOW/WiFi channel conflict | Radio/connectivity architecture phase | Test with phone hotspot on a channel deliberately different from the Aguada mesh channel; confirm graceful fallback, not silent packet loss |
| Flash wear / log format | Flash logging phase | Multi-day soak test measuring write latency trend + calculated erase-cycle budget documented against chosen cadence |
| Store-and-forward duplicates/loss | Store-and-forward sync phase | Repeated power/WiFi-kill-mid-batch test; verify zero dupes/gaps via monotonic id dedupe |
| NMEA fix-quality filtering | GPS acquisition/parsing phase | Poor-sky-view field test; inspect raw vs. filtered log for null-island/jump artifacts |
| VBAT/hot-start | Power architecture phase | Measure TTFF after real sleep/wake cycles in final enclosure; confirm hot-start (~1-3s), not cold-start (~30s+) |
| Automotive power transients | Power/hardware integration phase | Real-vehicle test through an engine crank event; confirm no reset/corruption |
| Adaptive cadence false-positive movement | Adaptive cadence phase | Multi-hour stationary test in real mount location; confirm low-rate state is reached and sustained |
| HA backdated history limitation | HA integration phase | Trigger a real batch sync after offline period; confirm live state uses live channel only, historical review is via GPX/external store, and this is documented as intended behavior, not a bug to chase |

## Sources

- [ESP-NOW & WiFi channel behavior — ESP32 Forum](https://www.esp32.com/viewtopic.php?t=12772)
- [esp-now channel setting in WiFi station mode — ESP32 Forum](https://www.esp32.com/viewtopic.php?t=14542)
- [ESP-NOW - ESP-FAQ (Espressif)](https://docs.espressif.com/projects/esp-faq/en/latest/application-solution/esp-now.html)
- [Wi-Fi API reference — ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html)
- [File System Considerations — ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/file-system-considerations.html)
- [Simple questions about SPIFFS wear levelling — ESP32 Forum](https://esp32.com/viewtopic.php?t=12921)
- [Wear Levelling API — ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/wear-levelling.html)
- [File management on ESP32: SPIFFS and LittleFS compared — Techrm](https://www.techrm.com/file-management-on-esp32-spiffs-and-littlefs-compared/)
- [In-Depth: Interface ublox NEO-6M GPS Module with Arduino — Last Minute Engineers](https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/)
- [GPS NEO-6M Accuracy: Tips to Improve Position Fix Quality — Zbotic](https://zbotic.in/gps-neo-6m-accuracy-tips-to-improve-position-fix-quality/)
- [Neo 6M GPS cold start and hot start duration — u-blox portal](https://portal.u-blox.com/s/question/0D52p0000E6RC95CQG/neo-6m-gps-cold-start-and-hot-start-duration)
- [How to Hot start neo-6m gps module — Arduino Forum](https://forum.arduino.cc/t/how-to-hot-start-neo-6m-gps-module/587872)
- [MQTT device tracker — Home Assistant docs](https://www.home-assistant.io/integrations/device_tracker.mqtt/)
- [Enter delayed (backdated) MQTT data asynchronously based on attached timestamp — HA Community](https://community.home-assistant.io/t/enter-delayed-aka-backdated-mqtt-data-asynchronously-based-on-attached-timestamp/750485)
- [Device Tracker implementation to import historical data with timestamp — HA Community](https://community.home-assistant.io/t/device-tracker-implementation-to-import-historical-data-with-timestamp/659019)
- Project context: `AGUADA_SYSTEM_DOC.md` (protocol v3, 16-byte packet), `.planning/PROJECT.md` (GPS tracker milestone scope), automotive load-dump/ISO 7637-2 transient class knowledge (general automotive electronics engineering practice)

---
*Pitfalls research for: ESP32 GPS vehicle/boat tracker, offline-first, no-GSM, opportunistic sync into Aguada ESP-NOW/MQTT/HA system*
*Researched: 2026-08-01*
