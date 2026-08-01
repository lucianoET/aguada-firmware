---
phase: 01-gps-acquisition-adaptive-cadence
plan: 01
subsystem: firmware
tags: [esp32, tinygpsplus, nmea, gps, embedded, platformio]

# Dependency graph
requires: []
provides:
  - "GpsReader — sole owner of the UART + TinyGPSPlus parser, emits an immutable Fix snapshot"
  - "Fix struct (include/gps_types.h) — the phase's contract with Phase 2's flash logger"
  - "FixGate — pure accept/reject decision over a Fix (9-member GateResult), rejects stale/implausible/low-quality fixes before anything downstream"
  - "gps_config.h — every gate/reader tunable as an #ifndef DEFAULT_GPS_* build_flags-overridable constant"
  - "Bench serial contract: [FIX]-successor [ACCEPT]/[REJECT] lines plus a periodic [HEALTH] line with per-reason reject counters"
affects: [01-02-cadence, phase-02-flash-logging]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "gps_reader owns UART + parser exclusively; every other module reaches GPS state only through the Fix snapshot"
    - "poll() checks TinyGPSPlus location.isUpdated() exactly once before any accessor with a side effect (FixQuality()/lat()/lng()), preventing a new-fix edge from being swallowed"
    - "fix_gate is a pure function over Fix — never touches Serial/HardwareSerial/the parser; its only library dependency is TinyGPSPlus::distanceBetween()"
    - "main.cpp forwards a gated fix downstream only inside the single GateResult::ACCEPT branch"
    - "main.cpp watchdog compensates for TinyGPSPlus's one-way isValid()/isUpdated() latch: when poll() returns false, elapsed time since the last real ACCEPT (or since boot, if never accepted) is compared against DEFAULT_GPS_FRESH_MS to surface REJECT_STALE/REJECT_NO_FIX in real time"

key-files:
  created:
    - firmware/gps_tracker/include/gps_config.h
    - firmware/gps_tracker/include/gps_types.h
    - firmware/gps_tracker/src/gps_reader.h
    - firmware/gps_tracker/src/gps_reader.cpp
    - firmware/gps_tracker/src/fix_gate.h
    - firmware/gps_tracker/src/fix_gate.cpp
  modified:
    - firmware/gps_tracker/src/main.cpp

key-decisions:
  - "gps_reader.poll() gates the entire Fix snapshot behind TinyGPSPlus's location.isUpdated() flag (per plan), which is structurally correct for genuine new fixes but means it can never observe a fix going stale after total lock loss — TinyGPSPlus's validity latch never re-fires. Compensated with a main.cpp-level watchdog (see Deviations) rather than re-polling extrapolated Fixes through FixGate::evaluate(), because feeding synthetic same-position Fixes through evaluate() every loop tick would corrupt the outlier-jump check's elapsed-time reference (lastMonoMs_ would slide forward on every tick instead of reflecting the true previous real fix)."
  - "HDOP fallback sentinel is 99.9f (an implausibly bad value) rather than 0, so an absent/invalid HDOP reading is rejected by fix_gate's HDOP-max check instead of silently passing as 'perfect'."
  - "UTC epoch conversion uses a hand-rolled days-from-civil algorithm (Howard Hinnant's public-domain formula) instead of mktime()/an external time library, keeping the conversion pure-UTC with no timezone handling, per the plan's explicit instruction."

requirements-completed: [GPS-01, GPS-02]

coverage:
  - id: D1
    description: "gps_reader owns UART + TinyGPSPlus parser; poll() emits an immutable Fix snapshot with lat/lon/speed/altitude/course/HDOP/sats/fix_quality/UTC timestamp, observable as [FIX]-successor [ACCEPT] lines on USB serial at 115200; same binary builds for both esp32-c3-supermini and esp32-devkit with no platformio.ini change (GPS-01)"
    requirement: "GPS-01"
    verification:
      - kind: other
        ref: "pio run -e esp32-c3-supermini && pio run -e esp32-devkit (both exit 0, no new warnings)"
        status: pass
      - kind: manual_procedural
        ref: "01-01-PLAN.md Task 1 human-check: flash bench board, confirm [FIX]/[ACCEPT] lines at ~1 Hz with real lat/lon/UTC epoch on both GPS modules"
        status: unknown
    human_judgment: true
    rationale: "Hardware verification (flashing the bench board, observing real NMEA fixes from both GPS modules) is explicitly out of scope for this execution session per environment notes — build-only verification was performed; the user runs the human-check steps later on the bench."
  - id: D2
    description: "fix_gate rejects fixes failing validity/freshness/null-island/UTC/HDOP/sats/warmup/plausibility checks with a named GateResult reason before anything reaches the single ACCEPT hand-off point; a fix frozen by loss of lock stops being accepted within the freshness window even though TinyGPSPlus still reports it valid (GPS-02)"
    requirement: "GPS-02"
    verification:
      - kind: other
        ref: "pio run -e esp32-c3-supermini && pio run -e esp32-devkit (both exit 0); source review confirms fix_gate.cpp has no Serial/HardwareSerial/parser reference and main.cpp only forwards inside GateResult::ACCEPT"
        status: pass
      - kind: manual_procedural
        ref: "01-01-PLAN.md Task 2 human-check: cold-boot REJECT_NO_FIX/REJECT_STALE, REJECT_WARMUP run before first ACCEPT, antenna-cover REJECT_STALE within ~2.5s, HEALTH per-reason counters rising"
        status: unknown
    human_judgment: true
    rationale: "Hardware verification (bench flashing, covering the antenna to confirm the staleness watchdog fires within the freshness window) is explicitly out of scope for this execution session per environment notes — the user runs the human-check steps later on the bench."

duration: 20min
completed: 2026-08-01
status: complete
---

# Phase 1 Plan 1: GPS Acquisition — gps_reader + fix_gate Summary

**Two-module GPS acquisition path (`gps_reader` owns UART+TinyGPSPlus behind an immutable `Fix` snapshot; `fix_gate` gates it through 9 named accept/reject outcomes) replacing the monolithic bench sketch, both target envs building clean.**

## Performance

- **Duration:** ~20 min
- **Started:** 2026-08-01T20:37:05Z (approx, per STATE.md session start)
- **Completed:** 2026-08-01T20:56:30Z
- **Tasks:** 2
- **Files modified:** 7 (5 created, 2 extended/rewired)

## Accomplishments
- `gps_reader` is now the sole owner of the `HardwareSerial` UART and the `TinyGPSPlus` parser instance; `main.cpp` reaches GPS state exclusively through the `Fix` struct and `GpsReader`'s counter methods (no direct TinyGPSPlus accessor calls)
- `poll()` checks `location.isUpdated()` exactly once before reading any parser accessor with a side effect, so a new-fix edge can never be swallowed by the GGA fix-quality accessor's updated-flag consumption
- UTC epoch (`utc_unix`) is derived at snapshot time via a hand-rolled days-from-civil helper — 0 when the date/time isn't yet valid, never touching `mktime()`/timezone logic
- `fix_gate` applies the full 8-check ordered pipeline (no-fix → stale → null-island → no-UTC → HDOP → sats → warmup → outlier jump via `TinyGPSPlus::distanceBetween`) as a pure function over the `Fix` snapshot, with zero dependency on `Serial`/`HardwareSerial`/the parser
- `main.cpp` forwards a gated fix downstream only inside the single, explicitly marked `GateResult::ACCEPT` branch — the structural guarantee Plan 02's cadence and Phase 2's flash logger inherit
- Added a main.cpp watchdog (documented as a deviation below) so a frozen fix actually stops being accepted within `DEFAULT_GPS_FRESH_MS`, compensating for TinyGPSPlus's one-way `isValid()`/`isUpdated()` latch which `poll()` alone cannot observe going stale
- Both `esp32-c3-supermini` and `esp32-devkit` PlatformIO envs build clean (no new warnings) with zero changes to `platformio.ini`

## Task Commits

1. **Task 1: gps_reader module — UART + parser ownership behind an immutable Fix snapshot (GPS-01)** - `5c9af74` (feat)
2. **Task 2: fix_gate module — reject stale, implausible and low-quality fixes before anything downstream (GPS-02)** - `098fac2` (feat)

**Plan metadata:** (this commit, docs)

## Files Created/Modified
- `firmware/gps_tracker/include/gps_config.h` - every gate/reader tunable as `#ifndef DEFAULT_GPS_*` (fresh-ms, min-UTC-year, health-period, bench-sim flag, HDOP max, sats min, warmup fixes, max plausible km/h)
- `firmware/gps_tracker/include/gps_types.h` - `Fix` struct (11 fields) and 9-member `GateResult` enum + `gateResultName()` declaration
- `firmware/gps_tracker/src/gps_reader.h` / `.cpp` - `GpsReader` class: `begin()`, `poll(Fix&)`, `bytesRead()`, `passedChecksum()`, `failedChecksum()`, `receiving()`, raw-echo toggle; read-only UART use by design
- `firmware/gps_tracker/src/fix_gate.h` / `.cpp` - `FixGate` class: `evaluate(const Fix&)`, `reset()`; `gateResultName()` implementation
- `firmware/gps_tracker/src/main.cpp` - bench harness wiring `GpsReader` → `FixGate` → serial (`[ACCEPT]`/`[REJECT]`/`[HEALTH]` lines), LED re-sourced from gate result, staleness watchdog

## Decisions Made
- HDOP fallback sentinel 99.9f (not 0) so an absent HDOP fails the max-HDOP check rather than passing as perfect
- Days-from-civil UTC conversion (no `mktime`, no timezone) per plan instruction
- Staleness/no-fix watchdog implemented as main.cpp-level time comparison against `DEFAULT_GPS_FRESH_MS`, not as repeated `FixGate::evaluate()` calls on synthetic Fixes — see Deviations below for why

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical Functionality] Added a main.cpp watchdog so REJECT_STALE/REJECT_NO_FIX are actually reachable in real time**
- **Found during:** Task 2 (fix_gate rewiring of main.cpp)
- **Issue:** Per Task 1's committed `poll()` contract (mandatory per that task's action text), `GpsReader::poll()` returns `true` only when TinyGPSPlus's `location.isUpdated()` flag fires — which requires a sentence that actually carried a fix (`sentenceHasFix`). Reading the vendored `TinyGPS++.cpp` directly confirms `location.commit()` (which sets `valid=updated=true`) is only ever called when `sentenceHasFix` is true, for both RMC and GGA, and nothing in the library ever resets `updated` back to `true` on its own — once a fix is genuinely lost mid-session (antenna covered), `sentenceHasFix` stays `false` forever until reacquisition, so `poll()` returns `false` forever too. With gate evaluation wired only to `poll()==true` events (as Task 2's action text literally specifies — "every snapshot returned by poll() goes through FixGate::evaluate()"), a frozen fix would keep displaying its last real `ACCEPT` state indefinitely: the LED would stay solid and no `REJECT_STALE` would ever print, directly violating this plan's own `must_haves` truth ("A fix frozen by loss of satellite lock stops being accepted within GPS freshness window") and threat `T-01-02` ("the highest-impact failure mode in this phase"). The same gap makes cold-boot `REJECT_NO_FIX`/`REJECT_STALE` unreachable too, since `location` never commits at all before the very first real 3D fix.
- **Fix:** Added a lightweight watchdog branch in `main.cpp`'s `loop()`, taken only when `poll()` returns `false` this tick: if no fix has ever been accepted, report `REJECT_NO_FIX`; if one was accepted before, compare `millis() - lastAcceptedAtMs` against the same `DEFAULT_GPS_FRESH_MS` constant `fix_gate` itself uses, and report `REJECT_STALE` once it's exceeded. This reuses the existing dedup-by-reason print/counter path so behavior stays consistent with the gated case. It deliberately does **not** re-invoke `FixGate::evaluate()` on a synthetic/extrapolated `Fix`: doing so on every loop tick while state is still fresh would repeatedly update `FixGate`'s internal `lastLat_/lastLon_/lastMonoMs_` outlier-check reference to "now" with unchanged coordinates, corrupting the elapsed-time delta used against the *next real* fix and risking false `REJECT_OUTLIER_JUMP`s on ordinary movement.
- **Files modified:** `firmware/gps_tracker/src/main.cpp`
- **Verification:** Both PlatformIO envs build clean; source review confirms the watchdog only ever sets `REJECT_NO_FIX`/`REJECT_STALE` (never `ACCEPT`) and never touches `FixGate`'s internal state — full behavioral confirmation (LED transition, log line, ~2.5s timing) requires the bench hardware step the user performs per the plan's human-check, not run in this session (see `## Known Stubs` — none; this is a verification-deferral, not a stub).
- **Committed in:** `098fac2` (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 Rule 2 — missing critical functionality)
**Impact on plan:** Necessary to satisfy GPS-02's stated behavior given TinyGPSPlus's documented one-way latch; no scope creep, no new files, no new build flags.

## Issues Encountered
None beyond the deviation above.

## User Setup Required
None - no external service configuration required. Hardware bench verification (both human-check sections in `01-01-PLAN.md`) is outstanding and left for the user, per this session's environment notes (build-only verification; hardware on `/dev/ttyACM0` not to be flashed by the executor).

## Next Phase Readiness
- `Fix` struct and `GateResult::ACCEPT` single hand-off point are in place and stable — Plan 02 (`cadence`) can attach directly at the marked call site in `main.cpp` without touching `gps_reader`/`fix_gate`
- Outstanding before Plan 02 lands: the user should run both plans' bench human-checks together (they use the same binary) to confirm real hardware behavior, including the antenna-cover staleness test that exercises this plan's new watchdog
- No blockers for Plan 02

---
*Phase: 01-gps-acquisition-adaptive-cadence*
*Completed: 2026-08-01*
