# Roadmap: Aguada — Node GPS Tracker (sem GSM)

## Overview

Ships a new no-GSM GPS tracker node type for the Aguada mesh: a battery-backed ESP32 + NEO-6M/M8N prototype that never loses trajectory data. The build order follows the data path itself — first make the GPS produce trustworthy, well-paced fixes, then make those fixes durable on flash before any network exists, then teach the node to find a known WiFi network and dump its backlog reliably, then make that data visible and exportable from the server side, and finally validate the hybrid power supply and leave room in the hardware design for future peripherals. Live hotspot sync, ESP-NOW mesh position relay, channel arbitration, and deep sleep are deliberately deferred to v2 — this milestone proves the offline-first core loop (acquire → log → sync → show → export) end to end on one prototype.

## Phases

**Phase Numbering:**

- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: GPS Acquisition & Adaptive Cadence** - Node reads NEO-6M/M8N fixes, filters out bad ones, and paces logging to movement
- [ ] **Phase 01.1: Perifericos I2C e Display (INSERTED)** - OLED de status, acelerometro alimentando a cadencia, bussola e temp/umidade num barramento I2C com degradacao graciosa
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

**Plans**: 2/2 plans executed
Plans:

- [x] 01-01-PLAN.md — gps_reader + fix_gate: structured Fix snapshot from NMEA, quality gating before anything downstream (GPS-01, GPS-02)
- [x] 01-02-PLAN.md — cadence: speed-based debounced pacing, plus bench simulator and BENCH.md verification procedure (GPS-03)

### Phase 01.1: Perifericos I2C e Display (INSERTED)

**Goal**: O firmware `gps_tracker` ganha um barramento I2C compartilhado (GPIO6/GPIO7) com OLED de status ao vivo, acelerometro alimentando a cadencia, bussola com rumo calibravel e sensor de temperatura/umidade — com degradacao graciosa total, recuperacao automatica em runtime, e sem jamais bloquear o pipeline GPS existente.
**Depends on**: Phase 1
**Requirements**: D-01..D-14 (fase inserida sem REQ IDs formais — o contrato de escopo sao as decisoes travadas em `phases/01.1-perifericos-i2c-e-display/01.1-CONTEXT.md`)
**Success Criteria** (what must be TRUE):

  1. Qualquer subconjunto dos 4 modulos I2C (inclusive nenhum) permite boot e operacao completos; modulo ausente desliga so o proprio recurso e um modulo que cai em runtime volta sozinho em ~10 s, sem reboot (D-12/D-13).
  2. O acelerometro promove a cadencia de STATIONARY para MOVING em ~300 ms sem esperar o debounce de 3 fixes do GPS, e nenhum caminho derivado do accel consegue rebaixar o estado — so a velocidade do GPS rebaixa (D-05/D-06/D-07).
  3. O rumo vem da bussola abaixo de `DEFAULT_GPS_STATIONARY_KMH` e do course do GPS acima dele, com auto-deteccao HMC5883L/QMC5883L e calibracao hard-iron pelo comando serial `cal` persistida em NVS (D-08/D-09/D-10/D-11).
  4. O OLED mostra uma tela densa unica a 1 Hz alinhada ao fix, com tela de aquisicao ao vivo enquanto nao ha fix, unidade de velocidade selecionavel por build flag e marcacao de modulos offline (D-01/D-02/D-03/D-04/D-14).

**Plans**: 4/4 plans executed
Plans:
**Wave 1**

- [x] 01.1-01-PLAN.md — fundacao I2C: constantes DEFAULT_*, lib_deps, modulo i2c_bus com boot scan, saude por modulo e retry (D-12, D-13)

**Wave 2** *(blocked on Wave 1 completion)*

- [x] 01.1-02-PLAN.md — accel_sensor MPU6050 e Cadence::onAccelWake com autoridade assimetrica (D-05, D-06, D-07)

**Wave 3** *(blocked on Wave 2 completion)*

- [x] 01.1-03-PLAN.md — mag_sensor dual-chip com calibracao em NVS e arbitro de rumo, env_sensor HTU21D, comando serial `cal` (D-08, D-09, D-10, D-11, D-14)

**Wave 4** *(blocked on Wave 3 completion)*

- [x] 01.1-04-PLAN.md — display OLED com tela densa e tela de aquisicao, satelites em vista, e procedimento de bancada no BENCH.md (D-01, D-02, D-03, D-04)

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
Phases execute in numeric order: 1 → 01.1 → 2 → 3 → 4 → 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. GPS Acquisition & Adaptive Cadence | 2/2 | In Progress|  |
| 01.1. Perifericos I2C e Display (INSERTED) | 4/4 | In Progress|  |
| 2. Durable Flash Logging | 0/TBD | Not started | - |
| 3. Store & Forward Sync (Known WiFi) | 0/TBD | Not started | - |
| 4. Server Ingest, HA Live Tracking & GPX Export | 0/TBD | Not started | - |
| 5. Hybrid Power & Hardware Provisioning | 0/TBD | Not started | - |
