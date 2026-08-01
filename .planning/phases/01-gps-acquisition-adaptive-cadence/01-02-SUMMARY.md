---
phase: 01-gps-acquisition-adaptive-cadence
plan: 02
subsystem: firmware
tags: [esp32, cadence, hysteresis, gps, embedded, platformio, bench-tooling]

# Dependency graph
requires: ["01-01"]
provides:
  - "Cadence — speed-driven, hysteresis-debounced MOVING/STATIONARY state machine that paces which gate-accepted fixes are actually emitted downstream"
  - "CadenceState/CadenceAction enums (include/gps_types.h) — the phase's second contract with Phase 2's flash logger (which fixes to write, not just which fixes are valid)"
  - "Bench simulator (GPS_BENCH_SIM) — synthesizes 1 Hz fixes through the real FixGate::evaluate() -> handleAccept() pipeline, provable from a desk via serial commands"
  - "BENCH.md — verification procedure, log-line format reference, and field-calibration record"
affects: [phase-02-flash-logging]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "cadence classifies movement from Fix.speed_kmh only (receiver's own Doppler-derived velocity), never from position deltas — stationary position wander reads as continuous slow motion on these single-frequency receivers"
    - "hysteresis band (DEFAULT_GPS_STATIONARY_KMH < DEFAULT_GPS_MOVING_KMH) plus a same-direction-only debounce streak (DEFAULT_GPS_DEBOUNCE_FIXES); any non-confirming vote, including a sample inside the band, resets the pending streak to zero"
    - "cadence.onGatedFix() has a single call site in main.cpp, reached only from inside a GateResult::ACCEPT branch (via the shared handleAccept() helper used by both the real-fix path and the bench simulator) — an ungated or gate-rejected fix can never reach the state machine"
    - "all cadence interval arithmetic uses Fix.mono_ms, never utc_unix, which jumps discontinuously when the receiver first acquires the date"
    - "bench simulator is a pure test-double at the Fix-snapshot boundary: it builds a plausible synthetic Fix and pushes it through the identical FixGate::evaluate() -> handleAccept() pipeline real fixes use, never a parallel/shortcut path; every line it emits carries a [SIM] marker"

key-files:
  created:
    - firmware/gps_tracker/src/cadence.h
    - firmware/gps_tracker/src/cadence.cpp
    - firmware/gps_tracker/BENCH.md
  modified:
    - firmware/gps_tracker/include/gps_config.h
    - firmware/gps_tracker/include/gps_types.h
    - firmware/gps_tracker/src/main.cpp

key-decisions:
  - "Refactored the GateResult::ACCEPT branch body into a shared handleAccept(f, origin) helper (origin=\"\" for real fixes, \"[SIM] \" for the simulator) so cadence.onGatedFix() keeps a single call site total in main.cpp while still being reused by two entry points — satisfies Task 2's explicit instruction that the simulator 'reuses the existing gate-then-cadence call path rather than adding a second one' without duplicating the ACCEPT-branch logic. onGatedFix() is reachable only from inside code paths already gated on GateResult::ACCEPT (structural guarantee, verified by source review + T-01-09's acceptance criterion, not by literal lexical nesting)."
  - "Bench simulator's outlier-jump injection ('x') and null-island injection ('z') leave the simulator's own position continuity anchor (simLat_/simLon_) untouched on the injected tick — mirroring fix_gate's own behavior of never poisoning its last-accepted reference on a rejected sample — so the next normal tick resumes from where the simulated trajectory actually was, not from the injected spike."
  - "Simulator position advances via an equirectangular due-east approximation (metres-per-degree-longitude scaled by cos(lat)) rather than a true great-circle projection — adequate at bench/vehicle scale near the equator and avoids pulling in a bearing/destination-point library dependency for a bench-only tool."
  - "Stationary heartbeat (DEFAULT_GPS_STATIONARY_HEARTBEAT_S) defaults to 0 (disabled) per the plan — GPS-03 only requires that acceptance pauses when parked; the constant exists for Phase 2 to enable a low-rate 'still parked' record later."

requirements-completed: [GPS-03]

coverage:
  - id: D3
    description: "cadence classifies MOVING/STATIONARY from Fix.speed_kmh using a hysteresis band (DEFAULT_GPS_MOVING_KMH=5.0 / DEFAULT_GPS_STATIONARY_KMH=3.0, both clear of the bench-observed ~2.6 km/h jitter) plus a DEFAULT_GPS_DEBOUNCE_FIXES=3 consecutive-vote debounce before flipping state; only gate-accepted fixes ever reach it (single onGatedFix call site, structurally reachable only from GateResult::ACCEPT)"
    requirement: "GPS-03 (state machine half)"
    verification:
      - kind: other
        ref: "pio run -e esp32-c3-supermini && pio run -e esp32-devkit (both exit 0); grep -c 'onGatedFix' src/main.cpp == 1; source review confirms cadence.cpp has no Serial/HardwareSerial/TinyGPS/distanceBetween reference and decides only from speed_kmh + mono_ms; Cadence's member initializer sets state_ = CadenceState::STATIONARY"
        status: pass
      - kind: manual_procedural
        ref: "01-02-PLAN.md Task 1 human-check (stationary hold with no [STATE] flapping over several minutes; MOVING transition only after consecutive fast fixes; mirror transition back) and Task 2 human-check (simulator-driven +/- speed walk through the same transitions)"
        status: unknown
    human_judgment: true
    rationale: "Hardware verification (flashing the bench board, observing real GPS-driven and simulator-driven transitions on the serial monitor) is explicitly out of scope for this execution session per environment notes — build-only verification was performed; the user runs both plans' human-check steps together on the bench, using the same binary."
  - id: D3b
    description: "in MOVING, cadence emits roughly once every DEFAULT_GPS_CADENCE_MOVING_S=8s from a 1 Hz input (immediate emit on entering MOVING, SUPPRESS_INTERVAL otherwise, all interval arithmetic on mono_ms); in STATIONARY, emission is fully suppressed (SUPPRESS_STATIONARY) since the Phase 1 default heartbeat interval is 0/disabled"
    requirement: "GPS-03 (pacing half)"
    verification:
      - kind: other
        ref: "pio run -e esp32-c3-supermini && pio run -e esp32-devkit (both exit 0); grep -c 'speed_kmh' src/cadence.cpp >=1, grep -c 'mono_ms' src/cadence.cpp >=2; source review confirms MOVING pacing gates on mono_ms delta vs DEFAULT_GPS_CADENCE_MOVING_S*1000 and STATIONARY pacing returns SUPPRESS_STATIONARY when the heartbeat constant is 0"
        status: pass
      - kind: manual_procedural
        ref: "01-02-PLAN.md Task 2 human-check: press 's' to start simulator, '+' past 5 km/h and confirm [STATE] after 3 confirming fixes then [EMIT] about every 8s with SUPPRESS_INTERVAL between; press '-' below 3 km/h and confirm the mirror transition and emission stop"
        status: unknown
    human_judgment: true
    rationale: "Same hardware-deferral rationale as D3 — the bench simulator makes this provable at a desk, but running it is a human-check step left for the user per this session's environment notes."
  - id: D4
    description: "bench simulator (GPS_BENCH_SIM-gated) synthesizes a plausible 1 Hz Fix stream (continuity-advanced due-east position consistent with the reported speed) and one-shot injected rejections (outlier jump, null island, bad HDOP/sats) via 's'/'+'/'-'/'x'/'z'/'b'/'h' serial commands, routing every synthetic fix through the identical FixGate::evaluate() -> handleAccept() pipeline real fixes use; every simulator line carries a [SIM] marker; BENCH.md documents the full verification procedure and a field-calibration record table"
    requirement: "GPS-03 (bench provability) + supports GPS-02 rejection-reason provability"
    verification:
      - kind: other
        ref: "pio run -e esp32-c3-supermini && pio run -e esp32-devkit (both exit 0); grep -c 'GPS_BENCH_SIM' src/main.cpp >=1; grep -c 'onGatedFix' src/main.cpp == 1 (simulator reuses the single call path); grep -c 'SIM' src/main.cpp >=1; grep -Ec section-heading pattern on BENCH.md >=6; grep -Ec tunable-name pattern on BENCH.md >=8; source review confirms the synthetic snapshot passes through FixGate::evaluate() before reaching cadence and every simulator output line is [SIM]-prefixed"
        status: pass
      - kind: manual_procedural
        ref: "01-02-PLAN.md Task 2 human-check: full 'Roteiro de verificacao da Fase 1' walkthrough via serial ('h', 's', '+', '-', 'x', 'z', 'b') plus filling BENCH.md's field-calibration table after a real stationary run"
        status: unknown
    human_judgment: true
    rationale: "Hardware verification of the actual serial interaction (pressing keys on a live monitor and reading the resulting log lines) is explicitly out of scope for this execution session per environment notes — the user runs the human-check steps and fills in BENCH.md's observed-value column later on the bench."

duration: 25min
completed: 2026-08-01
status: complete
---

# Phase 1 Plan 2: Cadence — speed-based adaptive pacing + bench simulator Summary

**`Cadence` module implements a hysteresis-debounced MOVING/STATIONARY state machine over gate-accepted fixes (emit ~8s while moving, suppress fully while parked), wired at Plan 01's single `GateResult::ACCEPT` hand-off point, plus a `GPS_BENCH_SIM`-gated serial-driven simulator and `BENCH.md` that make every gate rejection reason and every cadence transition provable from a desk.**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-08-01T20:59:00Z (approx, per STATE.md session start after Plan 01)
- **Completed:** 2026-08-01T21:24:00Z
- **Tasks:** 2
- **Files modified:** 6 (3 created, 3 extended/rewired)

## Accomplishments
- `Cadence` classifies each gate-accepted fix's `speed_kmh` against a hysteresis band (`DEFAULT_GPS_MOVING_KMH=5.0` / `DEFAULT_GPS_STATIONARY_KMH=3.0`, both clear of the bench-observed ~2.6 km/h stationary jitter) and only flips `MOVING`/`STATIONARY` after `DEFAULT_GPS_DEBOUNCE_FIXES=3` consecutive confirming votes — any non-confirming sample (including one inside the band) resets the pending streak to zero
- Pacing runs entirely on `mono_ms`: `MOVING` emits immediately on entering the state and then roughly every `DEFAULT_GPS_CADENCE_MOVING_S=8`s (`SUPPRESS_INTERVAL` in between); `STATIONARY` suppresses entirely (`SUPPRESS_STATIONARY`) since the Phase 1 default `DEFAULT_GPS_STATIONARY_HEARTBEAT_S=0` disables the heartbeat
- `main.cpp` wires cadence at Plan 01's single marked hand-off point inside `GateResult::ACCEPT`, replacing the plain `[ACCEPT]` print with `[EMIT]`/`[EMIT] hb=1`/`[SUPPRESS] reason=...`/`[STATE] OLD -> NEW` lines, and extends `[HEALTH]` with cadence state, seconds-since-emit, and cumulative emit/suppress counters
- Added a `GPS_BENCH_SIM`-gated bench simulator: `s`/`+`/`-`/`x`/`z`/`b`/`h` serial commands synthesize a continuity-advanced (due-east, speed-consistent) 1 Hz `Fix` stream or one-shot injected rejections, all routed through the identical `FixGate::evaluate()` → cadence pipeline real fixes use — refactored the ACCEPT-branch logic into a shared `handleAccept()` helper so `onGatedFix` keeps exactly one call site in `main.cpp` while being reused by both the real and simulated paths
- Every simulator-originated log line carries a `[SIM]` marker; real fixes are ignored for gate/cadence purposes while the simulator is active, though `gpsReader`'s byte/checksum counters keep accumulating into `[HEALTH]` regardless
- New `BENCH.md`: montagem pointer, build/flash commands, full serial-command table, log-line format reference, a 9-step Phase 1 verification script (marking which steps need real sky view vs. simulator-only), a two-module NMEA-talker confirmation table, and a field-calibration record table (one row per tunable, with assumption ID, observed-value and decision columns left for the operator)
- Both `esp32-c3-supermini` and `esp32-devkit` PlatformIO envs build clean (no new warnings) after both tasks, with zero changes to `platformio.ini`

## Task Commits

1. **Task 1: cadence module — speed-based, debounced pacing of accepted fixes (GPS-03)** - `6bad89e` (feat)
2. **Task 2: bench simulator and BENCH.md — prove gate and cadence from a desk, record field-tuning evidence** - `7ffcec9` (feat)

**Plan metadata:** (this commit, docs)

## Files Created/Modified
- `firmware/gps_tracker/include/gps_config.h` - added the five cadence tunables (`DEFAULT_GPS_MOVING_KMH`, `DEFAULT_GPS_STATIONARY_KMH`, `DEFAULT_GPS_DEBOUNCE_FIXES`, `DEFAULT_GPS_CADENCE_MOVING_S`, `DEFAULT_GPS_STATIONARY_HEARTBEAT_S`) behind `#ifndef DEFAULT_GPS_*` guards, with Assumption A3/A4/A5 caveats in comments
- `firmware/gps_tracker/include/gps_types.h` - added `CadenceState`/`CadenceAction` enums and `cadenceStateName()`/`cadenceActionName()` declarations
- `firmware/gps_tracker/src/cadence.h` / `.cpp` - new `Cadence` class: `onGatedFix()`, `state()`, `secondsSinceEmit()`, `stateChanged()` (one-shot), plus transition diagnostics (`lastTransitionSpeedKmh()`/`lastTransitionElapsedS()`) for the `[STATE]` log line
- `firmware/gps_tracker/src/main.cpp` - wired `Cadence` at the ACCEPT hand-off via a shared `handleAccept()` helper; added `GPS_BENCH_SIM`-gated bench simulator (state, serial command dispatch, `simTick()`); extended `[HEALTH]`
- `firmware/gps_tracker/BENCH.md` - new bench verification document (montagem, build/flash, comandos da serial, formato das linhas, roteiro de verificação, verificação com os dois módulos, registro de calibração de campo)

## Decisions Made
- Shared `handleAccept(f, origin)` helper keeps `cadence.onGatedFix()` to a single call site reused by both the real-fix path and the simulator, rather than duplicating the ACCEPT-branch logic to satisfy a literal "onGatedFix must be lexically inside `if (result == GateResult::ACCEPT)`" reading — both call sites are themselves gated on `GateResult::ACCEPT`, so the structural guarantee (T-01-09) holds by construction, verified via source review rather than lexical grep context
- Outlier-jump ('x') and null-island ('z') injections leave the simulator's continuity anchor untouched, mirroring `fix_gate`'s own never-poison-the-reference-on-reject behavior, so the next normal tick resumes from the real trajectory rather than the injected spike
- Simulator position advance uses an equirectangular due-east approximation (no bearing/destination-point library) — adequate at bench/vehicle scale and keeps the bench-only tool dependency-free
- Stationary heartbeat defaults to 0 (disabled) per plan — GPS-03 only requires suppression while parked; Phase 2 can enable a low-rate parked heartbeat later without touching cadence logic

## Deviations from Plan

None — plan executed as written. The `handleAccept()` refactor (see Decisions above) is exactly what Task 2's action text asked for ("the simulator reuses the existing gate-then-cadence call path rather than adding a second one"), not a deviation from it.

## Known Stubs

None. `BENCH.md`'s "Registro de calibração de campo" and "Verificação com os dois módulos" tables have blank observed-value cells by design — the plan explicitly specifies these as "an explicit place to record field-tuned thresholds" the operator fills in after real bench/field testing, not a stub blocking the plan's goal.

## Issues Encountered
- Initial draft of the top-of-file comment in `main.cpp` literally wrote `cadence.onGatedFix()`, which made `grep -c 'onGatedFix' src/main.cpp` return 2 instead of the required exactly-1 (Task 1's acceptance criterion). Reworded the comment to describe the call site without repeating the identifier; verified the grep count dropped to 1 before committing Task 1.

## User Setup Required
None - no external service configuration required. Hardware bench verification (both plans' human-check sections, run together since they share one binary per `01-01-SUMMARY.md`'s "Next Phase Readiness" note) is outstanding and left for the user, per this session's environment notes (build-only verification; hardware on `/dev/ttyACM0` not to be flashed by the executor). This includes filling in `BENCH.md`'s field-calibration table after a real multi-hour stationary test in the eventual mounting location.

## Next Phase Readiness
- `Cadence`'s `EMIT`/`EMIT_HEARTBEAT` outcomes are the exact hand-off Phase 2's flash logger needs — it can attach at the same points `[EMIT]`/`[EMIT] hb=1` print from, writing a trajectory record only when cadence actually releases a fix
- `DEFAULT_GPS_STATIONARY_HEARTBEAT_S` is deliberately 0/disabled in Phase 1; Phase 2 can turn on a low-rate "still parked" record by setting it nonzero without touching `cadence.cpp`
- Outstanding before Phase 1 is considered hardware-validated: the user runs both plans' bench human-checks together (real GPS lock/staleness tests from Plan 01, plus the simulator-driven cadence/rejection walkthrough from this plan) and records observed calibration values in `BENCH.md`
- No blockers for Phase 2

---
*Phase: 01-gps-acquisition-adaptive-cadence*
*Completed: 2026-08-01*

## Self-Check: PASSED

All 6 created/modified files confirmed present on disk (`gps_config.h`, `gps_types.h`, `cadence.h`, `cadence.cpp`, `main.cpp`, `BENCH.md`); task commits `6bad89e` and `7ffcec9` confirmed present in `git log`.
