# Roadmap: Aguada — Node GPS Tracker (sem GSM)

## Overview

Ships a new no-GSM GPS tracker node type for the Aguada mesh: a battery-backed ESP32 + NEO-6M/M8N prototype that never loses trajectory data. The build order follows the data path itself — first make the GPS produce trustworthy, well-paced fixes, then make those fixes durable on flash before any network exists, then teach the node to find a known WiFi network and dump its backlog reliably, then make that data visible and exportable from the server side, and finally validate the hybrid power supply and leave room in the hardware design for future peripherals. Live hotspot sync, ESP-NOW mesh position relay, channel arbitration, and deep sleep are deliberately deferred to v2 — this milestone proves the offline-first core loop (acquire → log → sync → show → export) end to end on one prototype.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: GPS Acquisition & Adaptive Cadence** - Node reads NEO-6M/M8N fixes, filters out bad ones, and paces logging to movement
- [ ] **Phase 2: Durable Flash Logging** - Every accepted fix survives reboot/power loss on flash before any network attempt
- [ ] **Phase 3: Store & Forward Sync (Known WiFi)** - Node autonomously finds known WiFi and uploads its backlog with zero loss/duplication
- [ ] **Phase 4: Server Ingest, HA Live Tracking & GPX Export** - Uploaded trajectory is stored, shown live on the HA map, and exportable as GPX
- [ ] **Phase 5: Hybrid Power & Hardware Provisioning** - Prototype runs on 12V/battery with visible power status and reserved pins for future peripherals

## Phase Details

### Phase 1: GPS Acquisition & Adaptive Cadence
**Goal**: The node reliably reads GPS fixes from the NEO-6M/M8N, rejects poor-quality fixes before they reach anything downstream, and paces fix acceptance to how the vehicle/boat is actually moving.
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: GPS-01, GPS-02, GPS-03
**Success Criteria** (what must be TRUE):
  1. Node parses NEO-6M/M8N NMEA output via UART and exposes lat/lon/speed/altitude/course plus a UTC timestamp for each valid fix.
  2. Fixes that fail quality gating (invalid, poor HDOP, too few satellites) are discarded and never reach the log or the cadence logic.
  3. While moving, a new fix is accepted roughly every 5-10 seconds; while stationary (GPS speed below a threshold for N seconds), fix acceptance pauses.
**Plans**: TBD

### Phase 2: Durable Flash Logging
**Goal**: Every accepted GPS fix is written to flash before any network attempt, and the log survives reboots and power loss without corruption or lost/duplicated upload progress.
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: LOG-01, LOG-02, LOG-03
**Success Criteria** (what must be TRUE):
  1. Every accepted fix is appended to a LittleFS binary record (fixed-size, segment rotation) before any network step occurs.
  2. After a reboot or power loss mid-write, the in-progress segment is intact and logging resumes cleanly with no corruption.
  3. The upload cursor is persisted to flash and only advances after the server confirms receipt, so restarts never lose or duplicate a point.
**Plans**: TBD

### Phase 3: Store & Forward Sync (Known WiFi)
**Goal**: The node autonomously detects a known WiFi network, connects without user intervention, and syncs its full backlog to the server, resuming safely after any interruption.
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: SYNC-01, SYNC-02, SYNC-03
**Success Criteria** (what must be TRUE):
  1. When a known SSID (credentials in NVS) comes into range, the node connects to it on its own.
  2. On connecting, the node uploads the entire pending backlog from the persisted cursor as JSON batches (MQTT/HTTP).
  3. If WiFi or MQTT drops mid-upload, the node retries with backoff and resumes the dump from the last confirmed cursor — no points lost or duplicated.
**Plans**: TBD

### Phase 4: Server Ingest, HA Live Tracking & GPX Export
**Goal**: Trajectory data uploaded by the node is durably stored server-side, visible live on the Home Assistant map, and exportable as a complete GPX track.
**Mode:** mvp
**Depends on**: Phase 3
**Requirements**: SRV-01, SRV-02, SRV-03
**Success Criteria** (what must be TRUE):
  1. The server ingests uploaded batches (extended `bridge.py` or a dedicated module) and stores the complete trajectory history keyed by `node_id`.
  2. Home Assistant shows the tracker as a `device_tracker` on the map via MQTT Discovery, with live latitude/longitude/speed/altitude/course attributes.
  3. Running `tools/gpx_export.py` against the ingested history produces a GPX file of the complete recorded trajectory — the source of truth, independent of HA's live-only view.
**Plans**: TBD

### Phase 5: Hybrid Power & Hardware Provisioning
**Goal**: The prototype runs reliably on hybrid 12V/battery power with power status visible in HA, and its physical design reserves room for planned future peripherals.
**Mode:** mvp
**Depends on**: Phase 1, Phase 4
**Requirements**: PWR-01, PWR-02, HW-01
**Success Criteria** (what must be TRUE):
  1. The tracker runs on 12V vehicle power when available and switches automatically to the Li-ion backup when 12V is removed, without a reboot or data-loss event.
  2. Battery voltage and active power source (external/battery) appear as attributes on the HA `device_tracker` entity.
  3. The prototype's pin/connector map documents reserved I2C, GPIO, and second-UART lines for future OLED/buttons/buzzer/NMEA-out integration without conflicting with current GPS/logging/power wiring.
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. GPS Acquisition & Adaptive Cadence | 0/TBD | Not started | - |
| 2. Durable Flash Logging | 0/TBD | Not started | - |
| 3. Store & Forward Sync (Known WiFi) | 0/TBD | Not started | - |
| 4. Server Ingest, HA Live Tracking & GPX Export | 0/TBD | Not started | - |
| 5. Hybrid Power & Hardware Provisioning | 0/TBD | Not started | - |
