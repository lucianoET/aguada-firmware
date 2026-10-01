---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
current_phase: 01.1
current_phase_name: perifericos-i2c-e-display
status: verifying
stopped_at: Completed 01.1-04-PLAN.md
last_updated: "2026-08-03T10:26:19.757Z"
last_activity: 2026-08-03
last_activity_desc: Phase 01.1 execution started
progress:
  total_phases: 2
  completed_phases: 2
  total_plans: 6
  completed_plans: 6
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-08-01)

**Core value:** Nenhum ponto do trajeto se perde: o tracker registra continuamente offline e sincroniza tudo sozinho assim que qualquer canal de conectividade aparece.
**Current focus:** Phase 01.1 — perifericos-i2c-e-display

## Current Position

Phase: 01.1 (perifericos-i2c-e-display) — EXECUTING
Plan: 4 of 4
Status: Phase complete — ready for verification
Last activity: 2026-09-30 — Completed quick task 260930-t20: bridge.py disponibilidade por sensor no HA quando chega pacote de erro

Progress: [██████████] 100%

## Performance Metrics

**Velocity:**

- Total plans completed: 0
- Average duration: - min
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**

- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
**Per-Plan Metrics:**

| Plan | Duration | Tasks | Files |
|------|----------|-------|-------|
| Phase 01 P01 | 20min | 2 tasks | 7 files |
| Phase 01 P02 | 25min | 2 tasks | 6 files |
| Phase 01.1 P01 | 40min | 3 tasks | 5 files |
| Phase 01.1 P02 | 25min | 3 tasks | 5 files |
| Phase 01.1 P03 | 30min | 3 tasks | 5 files |
| Phase 01.1 P04 | 45min | 3 tasks | 7 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Roadmap: Coarse granularity collapses research's 9 fine-grained phases into 5 vertical slices — v2 items (hotspot live sync, ESP-NOW mesh relay, channel arbitration, deep sleep) excluded entirely from v1 roadmap.
- Roadmap: Power/hardware work (Phase 5) placed last, after HA attribute wiring (Phase 4) exists for PWR-02 to report into.
- [Phase ?]: gps_reader.poll() gates the Fix snapshot on TinyGPSPlus location.isUpdated(); a main.cpp watchdog (not repeated FixGate::evaluate() calls) compensates for the library's one-way isValid()/isUpdated() latch to surface REJECT_STALE/REJECT_NO_FIX in real time without corrupting fix_gate's outlier-jump reference
- [Phase ?]: HDOP fallback sentinel is 99.9f (not 0) so an absent/invalid HDOP fails fix_gate's max-HDOP check instead of silently passing
- [Phase ?]: UTC epoch derived via a hand-rolled days-from-civil helper (no mktime, no timezone handling), computed only when GPS date/time are valid and year >= DEFAULT_GPS_MIN_UTC_YEAR
- [Phase ?]: cadence.onGatedFix() kept to a single call site via a shared handleAccept() helper reused by both the real-fix path and the bench simulator, rather than duplicating ACCEPT-branch logic; structural GateResult::ACCEPT gating verified by source review, not lexical nesting
- [Phase ?]: Bench simulator's outlier-jump/null-island injections leave the simulator's own position continuity anchor untouched on the injected tick, mirroring fix_gate's never-poison-the-reference-on-reject behavior
- [Phase ?]: Stationary heartbeat (DEFAULT_GPS_STATIONARY_HEARTBEAT_S) defaults to 0/disabled in Phase 1 per GPS-03's scope; Phase 2 can enable a low-rate parked heartbeat later without touching cadence.cpp
- [Phase ?]: Reused DEFAULT_GPS_STATIONARY_KMH as the future heading-arbiter threshold (D-08) instead of a new constant, per RESEARCH.md Pattern 3
- [Phase ?]: Cadence gains a second, promote-only entry point onAccelWake() alongside the GPS-gated onGatedFix(); only GPS speed can demote MOVING->STATIONARY, keeping engine vibration on a parked vehicle from holding the tracker falsely in MOVING (D-05).
- [Phase ?]: DFRobot_QMC5883 constructed with the i2c_bus::present()-detected address (not the library default 0x0C) — its begin() auto-detects ICType via hardcoded scan addresses, but subsequent reads use the constructor address, so they must match
- [Phase ?]: Added mag_sensor::calRange() beyond Task 1's 13-function contract so main.cpp's bench [CAL] active line can read live capture min/max without duplicating accumulator state
- [Phase 01.1]: GpsReader::satsInView() adds 3 mutable TinyGPSCustom members registered in the constructor (not header in-class init), preserving default-constructibility
- [Phase 01.1]: Display render scheduling adds an 80%-elapsed-and-fresh-fix anticipation rule alongside the 1Hz timer so the OLED frame never drifts a full second behind the GPS fix (D-02)
- [Phase 01.1]: pio CLI in this environment has no --project-option flag; PLATFORMIO_BUILD_FLAGS env var used instead to verify the DEFAULT_SPEED_UNIT=1 build

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 3/4 (sync + server ingest) carry MEDIUM confidence per research — WiFiMulti/PubSubClient patterns are standard but idempotent cursor/dedupe logic is flagged as the one shortcut this project cannot afford.
- Phase 5 (power) carries LOW confidence per research on automotive power protection component specifics — treat as a hardware spike requiring component-level research during planning.

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 260803-l28 | Node ethernet cabeado (Nano+ENC28J60+HC-SR04, MQTT aguada/raw, 0xEE02 CAV) + bridge.py aguada/raw/# | 2026-08-03 | 274b134 | [260803-l28-criar-node-ethernet-cabeado-aguada-firmw](./quick/260803-l28-criar-node-ethernet-cabeado-aguada-firmw/) |
| 260930-t20 | bridge.py: disponibilidade por sensor no HA quando chega pacote de erro (aguada/{node}/{sid}/availability, availability_mode all em nivel/pct/volume/distancia) | 2026-09-30 | f30dd4a | [260930-t20-bridge-py-disponibilidade-por-sensor-no-](./quick/260930-t20-bridge-py-disponibilidade-por-sensor-no-/) |

### Roadmap Evolution

- Phase 01.1 inserted after Phase 1: Perifericos I2C e Display: OLED SSD1306 status ao vivo, MPU6050 movimento p/ cadencia, HMC5883 rumo, HTU21 temp/umidade — modulos separados no barramento I2C (0x3C, 0x68, 0x1E, 0x40) (URGENT)

## Deferred Items

Items acknowledged and carried forward from previous milestone close:

| Category | Item | Status | Deferred At |
|----------|------|--------|-------------|
| *(none)* | | | |

## Session Continuity

Last session: 2026-08-03T10:26:19.726Z
Stopped at: Completed 01.1-04-PLAN.md
Resume file: None
