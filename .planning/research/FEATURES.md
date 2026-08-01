# Feature Research

**Domain:** ESP32 GPS vehicle/boat tracker, no GSM, opportunistic WiFi/ESP-NOW sync, Home Assistant integration
**Researched:** 2026-08-01
**Confidence:** MEDIUM-HIGH (reference projects verified directly; no-GSM opportunistic-sync category is a niche combination, so some patterns are synthesized from adjacent domains — DIY GPS loggers, ESPHome GPS component, MQTT device_tracker spec — rather than a single existing "no-GSM tracker" product)

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist in *any* GPS tracker, DIY or commercial. Missing these = product feels incomplete or unsafe to trust with "don't lose the trip" data.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| GPS fix acquisition (NMEA/UBX parse) | Baseline function of a "GPS tracker" | LOW | NEO-6M/M8N + TinyGPS++ or UBX binary parser; cold start 26–29s typical, warm/hot start faster with almanac retained |
| Lat/lon/speed/altitude/course capture | Minimum data a device_tracker or GPX consumer needs | LOW | All present in GPGGA/GPRMC/GPVTG sentences or UBX-NAV-PVT |
| Continuous offline trajectory logging to non-volatile storage | Core value stated in PROJECT.md — "no point of the trip is lost" | MEDIUM | Flash (internal SPIFFS/LittleFS or external SPI flash à la har-in-air) must survive power loss and reboot without corrupting the in-progress log |
| Fix-loss handling / dead-reckoning gap marking | GPS drops under bridges, tunnels, dense canopy, boat cabins | LOW-MEDIUM | Don't log garbage (0,0) fixes; mark gaps so GPX/HA don't render teleporting dots |
| GPX export of recorded track | Universal interchange format for Google Earth, Strava, QGIS, OsmAnd | LOW-MEDIUM | har-in-air's pattern (binary log → offline Python conversion) is proven; on-device GPX generation is also viable at this data volume |
| HA live map via `device_tracker` (MQTT Discovery) | This is *the* reason the feature is being added to the Aguada/HA ecosystem | LOW-MEDIUM | Use `json_attributes_topic` payload with `latitude`, `longitude`, `gps_accuracy`, plus custom attrs (speed, altitude, course, battery) — no need to hand-write YAML, HA auto-creates entity |
| Battery/power status visible to the user | Any unattended battery-backed device needs a "is it dying" signal | LOW | Voltage divider + ADC, exposed as HA attribute/sensor, same pattern as Aguada nodes' `vbat` field |
| Configurable known-WiFi credentials (SSID/password for hotspot + home/base network) | Store-and-forward and live-hotspot channels both require pre-provisioned networks | LOW | Reuse Aguada's existing captive-portal / NVS-config pattern already built for AirQ node |
| Timestamped points (even without RTC/NTP, relative+absolute reconciliation) | GPX and HA history both need real timestamps, not just sequence numbers | LOW-MEDIUM | GPS itself provides UTC time in the NMEA/UBX message — no need for a separate RTC chip |
| Graceful reconnect / retry on WiFi and MQTT drop | Cheap WiFi and mobile hotspots are flaky; a tracker that stalls on first disconnect fails the "syncs on its own" core value | MEDIUM | Standard exponential backoff, same class of problem the Aguada gateway/bridge.py already solves for ESP-NOW |
| Deep sleep / low-power idle when vehicle is off | Constraint explicitly stated in PROJECT.md — device lives on Li-ion when ignition is off | MEDIUM-HIGH | This is the single highest-complexity table-stakes item; see Pitfalls research for GPS-specific deep-sleep gotchas (cold start cost after full power-down vs. lighter sleep that keeps GPS backup power) |

### Differentiators (Competitive Advantage)

Features that set this tracker apart from generic commercial trackers (which are GSM-first) or from the reference DIY projects (which are single-channel).

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Three independent opportunistic sync channels (phone hotspot live, known-WiFi store-and-forward, ESP-NOW mesh) | No commercial or DIY tracker reviewed combines all three; most trackers are "always connected via GSM" or "always offline until manually downloaded" (har-in-air style). This is the actual novel value proposition of the milestone | HIGH | Needs a channel-priority/arbitration state machine: prefer live (hotspot) > bulk catch-up (known WiFi) > mesh position ping (ESP-NOW) when multiple are simultaneously available |
| Adaptive logging cadence (5–10s moving, pause when stationary) | Saves flash and battery vs. fixed-interval loggers (har-in-air uses fixed 1–60s or manual toggle; commercial trackers often log at fixed interval regardless of motion) | MEDIUM | Requires a motion/stationary classifier from GPS speed/HDOP alone (no IMU in scope) — speed-below-threshold-for-N-seconds is the standard DIY approach found in research |
| ESP-NOW position relay through existing Aguada mesh (no phone/WiFi required at all) | Turns the existing reservoir telemetry mesh into ad-hoc tracker backhaul — unique because it reuses infrastructure the user already owns on the island, unlike any commercial or reference product | HIGH | Requires protocol v3 extension (new packet type, since 16 bytes can't carry lat/lon/course/speed) — see PITFALLS/ARCHITECTURE for packing strategy |
| Full historical trajectory (not just last-known-position) surfaced through both HA and GPX | Car-Assistant (ShonP40) is live-position-only (ESPHome device_tracker, no track logging); har-in-air is track-logging-only (no live view). Combining both is the differentiator | MEDIUM | HA `device_tracker` only needs the *current* point; full history stays server-side (bridge.py or a small SQLite/file store) for GPX generation and later playback |
| Hybrid 12V + Li-ion power with automatic source switching and charge-state reporting | Car-Assistant's pattern (solar+battery+USB, reported as HA attributes) adapted to automotive 12V — differentiator vs. battery-only DIY loggers that die when the vehicle sits for days | MEDIUM | Needs simple ideal-diode-or, or MOSFET switch; report `power_source` (external/battery) and `battery_pct` as HA attributes, same style as Car-Assistant's charging status fields |
| Zero-provisioning join into existing Aguada node ID scheme | Reuses `node_id` = last 2 MAC bytes convention — tracker just becomes another "node" to the existing bridge.py/HA stack, no new integration surface | LOW | This is inherited "for free" from the brownfield decision to keep single-firmware + NVS-config architecture |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems in this specific no-GSM, single-prototype context — mirrors what PROJECT.md already puts Out of Scope, plus a few more surfaced by researching adjacent products.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| GSM/LTE fallback "just in case WiFi isn't available" | Every commercial tracker and Car-Assistant use cellular as the primary channel, so it feels like the "safe" default | Explicit user decision to exclude it; adds modem, SIM cost/logistics, antenna, power draw, and a whole new failure domain (carrier registration, data plan) that contradicts the "simple, opportunistic" design goal | Accept latency: live view only when hotspot is in range; everything else is store-and-forward, which is acceptable because full trajectory is never lost (buffered in flash) |
| WireGuard VPN / always-secure tunnel (Car-Assistant pattern) | Car-Assistant uses it because GSM = public untrusted network requiring a VPN back to home | Without GSM, the device only ever talks to networks the user already trusts (own hotspot, own home WiFi, own mesh) — VPN overhead (keys, tunnel maintenance, another service to keep alive on ESP32) has no matching threat model here | Plain MQTT over TLS (or even MQTT over a trusted LAN) is sufficient; keep credentials in NVS like the rest of Aguada |
| On-device live map / web UI on the tracker itself | Feels natural since the tracker "has WiFi anyway" and captive portal patterns already exist for AirQ node | Duplicates Home Assistant, which is already the system's single pane of glass; burns flash/RAM better spent on trajectory buffer; adds a second thing to keep in sync with reality | HA `device_tracker` + Lovelace map card is the single source of truth; captive portal reused only for Wi-Fi/MQTT provisioning, not for live maps (already captured as Out of Scope) |
| High-rate IMU/barometer/vario-style logging (500Hz IMU, 50Hz baro) as in har-in-air | The reference project the user cited does this, so it's tempting to imitate wholesale | That project is a paragliding vario — a completely different use case (flight dynamics) from a vehicle/boat position tracker; the sensor cost, storage rate, and processing burden are unjustified for road/water speed tracking | Borrow only the *logging-to-flash + WiFi-AP-download + offline GPX conversion* architecture pattern, not the sensor suite or sample rates — already captured correctly in PROJECT.md Out of Scope |
| Multi-vehicle fleet management / provisioning at scale in this milestone | Natural "why not build it right the first time" temptation | This milestone is explicitly one prototype to validate the concept; fleet features (bulk provisioning, per-vehicle geofencing rules, dashboards) are premature and would slow down validating the three-channel sync core value | Design the NVS config and MQTT topic scheme so that scaling to N trackers *later* is not blocked (e.g., keep node_id-keyed topics), but do not build fleet UI now |
| Geofencing / theft alerts / ignition-off alarms | Common in commercial trackers and would seem like an easy add-on once GPS + HA integration exist | Out of scope for this milestone's core value (never lose the trajectory); adds automation logic that belongs in HA (already has geofencing primitives) rather than firmware | Implement as HA automations against the `device_tracker` entity once live, not as firmware features |
| Continuous/aggressive polling of "known WiFi" networks while driving, to catch a fleeting connection | Seems like it maximizes sync opportunities | Wastes battery scanning for networks that are statistically absent while the vehicle/boat is in motion away from base; conflicts with the deep-sleep/motion-adaptive power budget | Scan for known networks only at a throttled cadence while moving, and more aggressively when stationary (i.e., likely parked at home/dock) — ties into the adaptive-cadence differentiator |

## Feature Dependencies

```
GPS fix acquisition (NMEA/UBX parse)
    └──requires──> UART wiring + baud config (NEO-6M/M8N default 9600, M8N often reconfigurable to higher baud)

Continuous offline trajectory logging (flash)
    └──requires──> GPS fix acquisition
    └──requires──> Flash storage layer (wear-aware ring buffer or append-log with wrap/rotate policy)

Adaptive logging cadence (moving vs. stationary)
    └──requires──> GPS fix acquisition (speed field from GPRMC/GPVTG or UBX-NAV-PVT)
    └──enhances──> Continuous offline trajectory logging (reduces flash writes, extends storage horizon)
    └──enhances──> Deep sleep / low-power idle (stationary detection is also the trigger to consider entering deeper sleep)

GPX export
    └──requires──> Continuous offline trajectory logging (flash)
    └──requires──> Timestamped points (GPX schema requires <time> per trackpoint)

HA device_tracker (MQTT Discovery)
    └──requires──> Live sync channel (hotspot) OR store-and-forward delivering recent points
    └──requires──> Lat/lon/speed/altitude/course capture
    └──enhances──> Battery/power status visible to user (surfaced as extra attributes, not just location)

Three opportunistic sync channels
    ├──Channel 1 (hotspot live)──requires──> known SSID/password config + internet reachability + MQTT client
    ├──Channel 2 (store-and-forward)──requires──> Continuous offline trajectory logging (flash) + known-WiFi credentials
    └──Channel 3 (ESP-NOW mesh)──requires──> Protocol v3 extension for position packet (16-byte packet has no room for lat/lon)
                                        └──requires──> Gateway/bridge.py updated to decode new packet type

Deep sleep / low-power idle
    └──requires──> Motion or ignition-sense wake source (RTC GPIO wake from external interrupt)
    └──conflicts──> Continuous high-rate GPS fix acquisition (GPS cold-start penalty after full power-down competes with sleep depth — see Pitfalls)

Hybrid 12V + Li-ion power with source switching
    └──enhances──> Deep sleep / low-power idle (removes the "battery must last the whole trip" pressure when 12V is present)
```

### Dependency Notes

- **Adaptive logging cadence requires GPS fix acquisition (speed field):** without IMU in scope, the only "am I moving" signal available is GPS-derived speed/HDOP, so this feature cannot be built before reliable fix parsing is solid — this pushes GPS parsing + filtering to an early phase.
- **Three sync channels require protocol v3 extension only for Channel 3:** Channels 1 and 2 explicitly bypass the 16-byte ESP-NOW packet (PROJECT.md confirms store-and-forward goes direct to MQTT/HTTP JSON), so the packet-format work is scoped *only* to the mesh-relay channel — this is a phase-ordering signal: Channels 1+2 can ship before the harder protocol-extension work for Channel 3.
- **Deep sleep conflicts with GPS cold-start cost:** a full power-off of the GPS module forces ephemeris/almanac reacquisition (up to ~30s cold start) on every wake, which both burns time-to-fix and battery — the standard mitigation is keeping GPS backup power (VBAT pin) alive across sleep so it retains almanac for a faster warm start; this must be decided before finalizing the sleep design, not left as an afterthought.
- **GPX export enhances but does not require HA device_tracker:** these two consumers of the same trajectory data are independent — GPX can be generated purely from the flash log without any live connectivity, so it should not be blocked on the sync-channel work landing first.

## MVP Definition

### Launch With (v1)

Minimum viable product — what's needed to validate "the trajectory is never lost and it syncs on its own."

- [ ] GPS fix acquisition + parsing (lat/lon/speed/altitude/course, UTC timestamp) — without this nothing else exists
- [ ] Continuous offline trajectory logging to flash, survives reboot/power loss — this *is* the core value statement
- [ ] Adaptive cadence (moving vs. stationary) — needed so flash/battery budget is credible for a real trip, not just a demo
- [ ] Channel 2 (store-and-forward on known WiFi) — simplest channel to prove end-to-end sync works, no live-hotspot timing dependency
- [ ] HA `device_tracker` via MQTT Discovery, fed from the store-and-forward dump — validates the HA integration pattern cited from Car-Assistant
- [ ] GPX export from the flash log (even if via an offline script initially, à la har-in-air) — validates the "trajectory is usable outside HA" requirement
- [ ] Basic battery voltage reporting — minimum viewability into whether the prototype is about to die

### Add After Validation (v1.x)

Features to add once the core store-and-forward + logging loop is proven on real trips.

- [ ] Channel 1 (phone hotspot live sync) — add once store-and-forward is reliable; trigger: user wants to see the boat/car moving in real time, not just after the fact
- [ ] Channel 3 (ESP-NOW mesh relay) — trigger: protocol v3 extension is designed and the prototype needs to prove connectivity works even without any WiFi at all (deep-mesh scenario, e.g., mid-island with no hotspot)
- [ ] Deep sleep with motion/ignition wake — trigger: once logging+sync logic is stable, then optimize for the "vehicle off for days" battery-survival case; don't let power optimization block functional validation
- [ ] On-device GPX generation (vs. offline script) — trigger: once the offline conversion pipeline (borrowed from har-in-air) proves the format/fields are right, move it on-device for convenience
- [ ] Channel arbitration/priority logic when multiple channels are available simultaneously — trigger: once all three channels individually work, decide how they interact

### Future Consideration (v2+)

Features to defer until the single-prototype concept is validated and fleet-scale or richer features are in scope.

- [ ] Multi-tracker fleet provisioning — defer until this prototype validates the 3-channel sync model
- [ ] Geofencing / arrival-departure automations — defer to HA-side automation, not firmware, and only after live tracking is stable
- [ ] OTA firmware updates over WiFi (har-in-air has this) — defer; not blocking the core value, existing Aguada nodes don't have OTA yet either
- [ ] Richer power telemetry (solar input, charge curves, like Car-Assistant) — only relevant if a solar-charging variant is built later

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| GPS fix acquisition + parsing | HIGH | LOW | P1 |
| Continuous offline trajectory logging (flash) | HIGH | MEDIUM | P1 |
| Adaptive logging cadence | HIGH | MEDIUM | P1 |
| Channel 2 — store-and-forward on known WiFi | HIGH | MEDIUM | P1 |
| HA device_tracker via MQTT Discovery | HIGH | LOW-MEDIUM | P1 |
| GPX export | HIGH | LOW-MEDIUM | P1 |
| Battery voltage reporting | MEDIUM | LOW | P1 |
| Channel 1 — phone hotspot live sync | HIGH | MEDIUM | P2 |
| Channel 3 — ESP-NOW mesh relay + protocol v3 extension | HIGH (unique differentiator) | HIGH | P2 |
| Deep sleep / motion or ignition wake | MEDIUM-HIGH (battery survival) | HIGH | P2 |
| Hybrid 12V + Li-ion power switching | MEDIUM | MEDIUM | P2 |
| Channel arbitration/priority state machine | MEDIUM | MEDIUM | P2 |
| On-device GPX generation | LOW-MEDIUM | LOW-MEDIUM | P3 |
| Fleet provisioning | LOW (this milestone) | HIGH | P3 |
| Geofencing/theft alerts | LOW (belongs in HA) | LOW (in HA) | P3 |
| GSM fallback | N/A — explicitly rejected | — | Rejected |
| WireGuard VPN | N/A — explicitly rejected | — | Rejected |

**Priority key:**
- P1: Must have for launch (proves core value: nothing lost, syncs on its own)
- P2: Should have, add when possible (proves the *differentiating* three-channel/mesh/power story)
- P3: Nice to have, future consideration

## Competitor Feature Analysis

| Feature | Car-Assistant (ShonP40) | ESP32_IMU_BARO_GPS_VARIO (har-in-air) | Our Approach |
|---------|--------------------------|----------------------------------------|--------------|
| Connectivity | LTE/GSM modem (T-SIM7600) as primary; WiFi hotspot + WireGuard VPN as secondary | None — pure offline logger with WiFi AP only for local download, no live connectivity | Three opportunistic WiFi/ESP-NOW channels, no cellular at all — deliberately positioned between these two extremes |
| Trajectory history | Not a focus — live position only, no persistent track log described | Core feature — flash-buffered high-rate log, offline GPX conversion | Combine both: live-ish position (via HA) AND full persistent trajectory (via flash + GPX), which neither reference project does alone |
| HA integration | Native via ESPHome `device_tracker`/sensors, blueprint automations (e.g., GPS-failure restart) | None — no HA integration, web UI only | Adopt Car-Assistant's MQTT/HA attribute pattern (lat/lon/speed/altitude/battery) but implement via raw MQTT Discovery (not ESPHome) to match Aguada's existing bridge.py architecture |
| Data retrieval | Remote-only, over cellular/VPN — no local flash dump workflow | WiFi AP + web page manual download; offline Python script splits and converts to GPX | Store-and-forward channel automates what har-in-air does manually — no user has to walk up to the tracker and click "download" |
| Power management | Solar + battery + USB charging reported as HA attributes; no aggressive sleep (always-on for cellular registration) | No explicit deep sleep; ~100–150mA continuous draw acceptable for flight-duration use | Requires actual deep sleep since PROJECT.md's use case leaves the tracker unattended for days (vehicle parked); neither reference project needed this |
| Packet/data format | Standard ESPHome sensor entities over its own MQTT/API, no custom binary framing | Custom binary log format on SPI flash, custom offline GPX converter | Reuse the har-in-air binary-log-to-offline-GPX shape for the flash format, but must additionally define a new ESP-NOW v3 packet type for the mesh channel, which neither reference project needed (neither had a mesh) |

## Sources

- [ShonP40/Car-Assistant (GitHub)](https://github.com/ShonP40/Car-Assistant) — HA/ESPHome integration pattern, sensor/attribute set, connectivity architecture (LTE + WireGuard), power/charging reporting. Confidence: HIGH (primary source, user-cited reference project).
- [har-in-air/ESP32_IMU_BARO_GPS_VARIO README (GitHub)](https://github.com/har-in-air/ESP32_IMU_BARO_GPS_VARIO/blob/master/README.md) — flash logging behavior, GPS track interval configurability, WiFi AP download workflow, offline GPX conversion pipeline, power draw figures. Confidence: HIGH (primary source, user-cited reference project).
- [Home Assistant MQTT device_tracker documentation](https://www.home-assistant.io/integrations/device_tracker.mqtt/) — `json_attributes_topic` payload schema (`latitude`, `longitude`, `gps_accuracy`, `battery_level`), MQTT Discovery config shape. Confidence: HIGH (official docs).
- [ESPHome GPS component docs](https://esphome.io/components/gps/) — baseline expectations for GPS-derived `device_tracker` entities in the HA ecosystem. Confidence: MEDIUM-HIGH (official docs, adjacent framework not directly used but same integration target).
- Community/DIY GPS logger discussions (Home Assistant Community forum threads on MQTT device_tracker configuration; general DIY GPS logger distance/time-threshold logging patterns; ESP32 deep-sleep wake-source guides from Zbotic/RandomNerdTutorials/Electropeak). Confidence: MEDIUM (aggregated community/tutorial sources, cross-checked across multiple independent write-ups, no single canonical spec for "adaptive GPS logging cadence").
- PROJECT.md (`.planning/PROJECT.md`) — milestone requirements, constraints, and explicit Out of Scope decisions used to calibrate table-stakes vs. anti-features. Confidence: HIGH (project's own source of truth).

---
*Feature research for: ESP32 GPS vehicle/boat tracker, no-GSM, opportunistic sync (Aguada system extension)*
*Researched: 2026-08-01*
