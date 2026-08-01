---
phase: 01-gps-acquisition-adaptive-cadence
verified: 2026-08-01T21:52:08Z
status: human_needed
score: 1/8 must-haves verified (source-verified structural truths), 7 present, behavior-unverified
behavior_unverified: 7
overrides_applied: 0
mvp_mode_guard: FAILED — see "MVP Mode Guard" note below. Standard goal-backward verification was run instead (roadmap Success Criteria are well-formed and used as the authoritative contract).
behavior_unverified_items:
  - truth: "Bench operator sees one structured line per GPS fix on USB serial 115200 carrying lat, lon, altitude, speed, course and a UTC timestamp (GPS-01)."
    test: "Flash esp32-c3-supermini, open `pio device monitor` at 115200, get a real sky fix."
    expected: "`[EMIT]`/`[SUPPRESS]` lines (successor to the plan's `[FIX]`/`[ACCEPT]` naming — see Deviation note) show t=, lat=, lon=, alt=, spd=, crs=, hdop=, sats=, q=, age=, state= once per accepted fix."
    why_human: "Requires live NMEA input and a serial monitor; no test harness or hardware access in this session."
  - truth: "A fix that is stale, has no GGA fix quality, sits at (0,0), has HDOP above the limit, too few satellites, an invalid UTC date, or implies an impossible jump is printed as a REJECT with a named reason and does not propagate downstream (GPS-02)."
    test: "Cold boot with no sky view, then use bench simulator `x`/`z`/`b` commands per BENCH.md step 4."
    expected: "`[REJECT] reason=REJECT_*` lines with the matching name; no `[EMIT]`/`[SUPPRESS]` (i.e. no cadence involvement) for a rejected fix."
    why_human: "Live serial observation required; source review already confirms the 8-step ordered gate and the single downstream hand-off point exist and are wired correctly (see Findings)."
  - truth: "A fix frozen by loss of satellite lock stops being accepted within GPS freshness window, even though TinyGPSPlus still reports it valid."
    test: "Acquire a real fix, then cover the antenna / unplug GPS TX and time how long until the log flips to `REJECT_STALE`."
    expected: "Within ~`DEFAULT_GPS_FRESH_MS` (2500 ms) of losing lock, `[REJECT] reason=REJECT_STALE` appears and the LED stops being solid."
    why_human: "This is a runtime staleness/cancellation invariant — no automated test exists in this repo (no test suite) to exercise it; requires timed physical observation. Source review confirms the compensating main.cpp watchdog (documented as an auto-fixed deviation in 01-01-SUMMARY.md) is present and structurally sound, but its ~2.5s timing behavior is unexercised by any test."
  - truth: "While the tracker is moving, exactly one fix is emitted roughly every 8 seconds even though the receiver delivers one fix per second."
    test: "BENCH.md step 6-7: press `s` then `+` repeatedly past 5 km/h."
    expected: "`[STATE] STATIONARY -> MOVING` after 3 confirming fixes, then `[EMIT]` about every 8s with `SUPPRESS_INTERVAL` on the intervening ~7 fixes."
    why_human: "Timed pacing behavior; no automated test exercises `Cadence::onGatedFix`'s interval arithmetic at runtime."
  - truth: "While the tracker is stationary, emission stops entirely and the log says so, without the moving/stationary state flapping on GPS speed jitter."
    test: "BENCH.md step 5: simulator at 0 km/h (or real stationary bench sit) for several minutes."
    expected: "`[SUPPRESS] reason=SUPPRESS_STATIONARY` repeats with no `[STATE]` line over several minutes, despite the bench-observed ~2.6 km/h jitter floor."
    why_human: "No-flapping is an absence-of-event claim over time; requires live/timed observation, not statically provable from source alone."
  - truth: "The moving/stationary transition requires several consecutive confirming fixes and a hysteresis gap, so a single jittery sample cannot flip the state."
    test: "BENCH.md steps 6/8: walk the simulated speed up/down across the 3.0-5.0 km/h band one `+`/`-` press at a time."
    expected: "A single sample landing inside the hysteresis band (or a single non-confirming vote) resets the pending debounce streak to zero and does not flip state."
    why_human: "Debounce-streak-reset-on-non-confirmation is a state-machine invariant; source review confirms the logic (`cadence.cpp` lines 39-64) but no test exercises it at runtime."
  - truth: "A bench operator can prove all of the above, plus each gate rejection reason, from a desk with no vehicle, using serial commands."
    test: "Full BENCH.md 'Roteiro de verificação da Fase 1' (9 steps) plus the two-module NMEA-talker confirmation and field-calibration table."
    expected: "Every rejection reason and every cadence transition is reproducible via `h`/`s`/`+`/`-`/`x`/`z`/`b`, and `BENCH.md`'s calibration table gets filled in after a real multi-hour stationary test."
    why_human: "Requires an operator with the physical board and a serial monitor; this is exactly the class of check this session's environment notes exclude from automated verification."
human_verification:
  - test: "MVP mode guard: run `/gsd mvp-phase 1` to canonicalize the ROADMAP Phase 1 goal into User Story form, or confirm standard (non-MVP) verification is acceptable for this phase."
    expected: "Either ROADMAP.md's Phase 1 `**Goal:**` line becomes an `As a ... I want to ... so that ...` sentence, or the team explicitly accepts that this firmware phase does not fit the MVP user-flow framing and should stay on standard goal-backward verification."
    why_human: "`user-story.validate` returned `valid: false` for the current ROADMAP goal text while ROADMAP.md declares `Mode: mvp` — per the verifier's mvp_mode_verification contract this is a discrepancy requiring a human decision, not something the verifier can resolve unilaterally."
  - test: "Flash `esp32-c3-supermini` (`/dev/ttyACM0`) and `esp32-devkit`, open `pio device monitor` at 115200, and follow `firmware/gps_tracker/BENCH.md`'s 'Roteiro de verificação da Fase 1' end to end (9 steps: cold boot, warmup, first accept + antenna-cover staleness, injected rejections via `x`/`z`/`b`, stationary suppression, moving transition, ~8s cadence, moving->stationary transition, two-module NMEA confirmation)."
    expected: "Every named `GateResult`/`CadenceAction` is reachable and printed with the correct prefix (`[REJECT]`/`[EMIT]`/`[SUPPRESS]`/`[STATE]`/`[HEALTH]`), timings roughly match the constants in `gps_config.h` (2500ms stale window, 8s cadence, 3-fix debounce), and the LED tracks gate/cadence state as documented."
    why_human: "Live hardware + timed serial observation; no test harness exists in this repo (validation is on-hardware only per CLAUDE.md) and this session's environment notes explicitly exclude flashing/observing hardware."
  - test: "Specifically re-exercise the CR-02/WR-02 regression path: run the bench simulator with movement (`s`, `+` past 5 km/h, let it emit a few times), then press `s` again to stop it, and confirm real fixes resume driving `[EMIT]`/`[REJECT]` without a permanent `REJECT_OUTLIER_JUMP` lockout."
    expected: "After the simulator is turned off, the very next real accepted fix is evaluated against a freshly-reset `FixGate` reference (per the `fixGate.reset()` call added in commit `a63bead`) rather than against the simulator's drifted last position, so it is not spuriously rejected forever."
    why_human: "This is exactly the failure mode 01-REVIEW.md's CR-02 finding described and fixed; the fix is structurally verified by source review (grep shows `fixGate.reset()` in both branches of `simToggle()`), but the actual sim-to-real handoff has not been exercised on hardware since the fix landed."
---

# Phase 1: GPS Acquisition & Adaptive Cadence Verification Report

**Phase Goal:** The node reliably reads GPS fixes from the NEO-6M/M8N, rejects poor-quality fixes before they reach anything downstream, and paces fix acceptance to how the vehicle/boat is actually moving.
**Verified:** 2026-08-01T21:52:08Z
**Status:** human_needed
**Re-verification:** No — initial verification

## MVP Mode Guard

ROADMAP.md declares `**Mode:** mvp` for Phase 1, but its `**Goal:**` line ("The node reliably reads GPS fixes...") is not in canonical User Story form (`gsd_run query user-story.validate --pick valid` → `false`). Per the verifier's `mvp_mode_verification` contract, this is a discrepancy that should be surfaced rather than silently patched over with a low-quality "User Flow Coverage" table built from a non-conforming goal string.

**Resolution taken for this report:** the MVP "User Flow Coverage" framing was skipped. Standard goal-backward verification was performed instead, using ROADMAP.md's `Success Criteria` (the roadmap contract, per Step 2a — always authoritative regardless of PLAN frontmatter) merged with both plans' `must_haves.truths`. This is flagged as a `human_verification` item above; it does not block the rest of this report because the roadmap Success Criteria are well-formed, unambiguous, and give a complete verification target on their own. Recommended action: run `/gsd mvp-phase 1` to canonicalize the goal, or confirm the team is fine with this firmware phase staying on standard (non-MVP) verification going forward.

## Goal Achievement

### Observable Truths

Merged from ROADMAP Success Criteria (SC1-SC3) and both plans' `must_haves.truths`, deduplicated.

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | GPS-01 — parses NMEA, exposes lat/lon/speed/altitude/course + UTC timestamp per fix on serial (SC1) | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | `Fix` struct carries all 6 fields (`gps_types.h:8-24`); `GpsReader::poll()` populates every field from validated TinyGPSPlus accessors with documented fallbacks (`gps_reader.cpp:56-81`); `printEmit()` formats all of `t=/lat=/lon=/alt=/spd=/crs=/hdop=/sats=/q=/age=/state=` (`main.cpp:84-91`). Live serial confirmation not performed this session. |
| 2 | GPS-02 — fixes failing quality gating are discarded with a named reason and never reach downstream (SC2) | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | `FixGate::evaluate()` implements the full 8-check ordered pipeline (no-fix → stale → null-island → no-UTC → HDOP → sats → warmup → outlier-jump), returns a named `GateResult` for every path (`fix_gate.cpp:25-93`); `main.cpp` forwards downstream only inside `GateResult::ACCEPT` branches (both real-fix `loop()` at line 363 and simulator `simTick()` at line 308), `grep -c 'GateResult::ACCEPT' src/main.cpp` = 8 (all inside gate-result checks, none elsewhere), `onGatedFix` appears exactly once (`main.cpp:117`, inside `handleAccept()`, itself only called from `GateResult::ACCEPT` branches). Structural guarantee is source-verified; live REJECT-line observation not performed this session. |
| 3 | A fix frozen by loss of satellite lock stops being accepted within the freshness window even though TinyGPSPlus still reports it valid | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | `fix_gate.cpp:38-41` gates on `age_ms >= DEFAULT_GPS_FRESH_MS`. Because `GpsReader::poll()` only returns `true` on a genuine `isUpdated()` edge (per plan contract) and TinyGPSPlus's validity flag never re-fires once frozen, `01-01-SUMMARY.md` documents an explicit main.cpp-level watchdog (lines 368-396) that surfaces `REJECT_STALE`/`REJECT_NO_FIX` in real time by comparing `millis() - lastAcceptedAtMs` against the same constant. Source-verified as present and structurally sound (does not touch `FixGate`'s internal state, per the documented rationale); the ~2500ms timing behavior itself is unexercised by any test — this repo has no automated test suite (validation is on-hardware only per CLAUDE.md). |
| 4 | Firmware still builds and runs unchanged on both the ESP32-C3 SuperMini and ESP32 DevKit target boards | ✓ VERIFIED | `pio run -e esp32-c3-supermini` and `pio run -e esp32-devkit` both re-run and exit 0 in this session (SUCCESS, 5.15s / 8.50s, no new warnings); `git diff --stat -- platformio.ini` empty (no build-flag/env changes). Matches both plans' literal acceptance criteria (build-only). "Runs on hardware" is covered by the human-verification bench walkthrough below. |
| 5 | While moving, a new fix is accepted/emitted roughly every 5-10s (SC3, target 8s) | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | `cadence.cpp:67-77`: in `MOVING`, emits immediately on entering the state, then gates on `f.mono_ms - lastEmitMs_ >= DEFAULT_GPS_CADENCE_MOVING_S*1000` (8000ms default), else `SUPPRESS_INTERVAL`. Source-verified logic and constant; runtime pacing not exercised this session. |
| 6 | While stationary, fix acceptance pauses entirely, without state flapping on GPS speed jitter (SC3) | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | `cadence.cpp:79-90`: `STATIONARY` returns `SUPPRESS_STATIONARY` unless the (disabled-by-default, `=0`) heartbeat interval elapsed. Hysteresis band (`DEFAULT_GPS_MOVING_KMH=5.0` / `DEFAULT_GPS_STATIONARY_KMH=3.0`) documented as clearing the bench-observed ~2.6 km/h jitter floor (`01-RESEARCH.md` Pitfall 5, referenced in `gps_config.h:58-64`). No-flapping-over-time is an absence-of-event claim; not exercisable statically. |
| 7 | Moving/stationary transition requires several consecutive confirming fixes and a hysteresis gap; a single jittery sample cannot flip the state | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | `cadence.cpp:39-64`: any vote that does not confirm the pending transition (including one inside the band) resets the opposing streak to zero every call; only an uninterrupted run of `DEFAULT_GPS_DEBOUNCE_FIXES` (3) confirming votes flips `state_`. Source-verified against the plan's exact spec; the actual anti-flap behavior under real jitter is unexercised by a test. |
| 8 | A bench operator can prove all of the above, plus each gate rejection reason, from a desk with no vehicle, using serial commands | ⚠️ PRESENT_BEHAVIOR_UNVERIFIED | `GPS_BENCH_SIM`-gated simulator (`main.cpp:142-322`) implements `h`/`s`/`+`/`-`/`x`/`z`/`b`; every synthetic fix routes through the identical `FixGate::evaluate()` → `handleAccept()` pipeline (`simTick()` line 303/309, same call as the real path at line 359/364); `onGatedFix` remains exactly 1 occurrence codebase-wide, confirming the simulator adds no second call site. `BENCH.md` documents a 9-step "Roteiro de verificação da Fase 1" plus a field-calibration record. Structurally sound and complete; an actual desk run was not performed this session. |

**Score:** 1/8 truths verified programmatically; 7 present and wired but behavior-unverified (require live hardware/serial observation per this session's environment notes — see `human_verification` above).

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `firmware/gps_tracker/include/gps_config.h` | Every tunable behind `#ifndef DEFAULT_GPS_*` | ✓ VERIFIED | 12 `#ifndef DEFAULT_GPS_*` guards covering reader/gate/cadence tunables plus `GPS_BENCH_SIM`; all referenced by consuming modules. |
| `firmware/gps_tracker/include/gps_types.h` | `Fix` struct (11 fields), `GateResult` (9 members), `CadenceState`/`CadenceAction` | ✓ VERIFIED | All types present with the exact member sets specified in both plans; free-function name declarations (`gateResultName`, `cadenceStateName`, `cadenceActionName`) present and implemented. |
| `firmware/gps_tracker/src/gps_reader.h/.cpp` | Sole UART+parser owner, immutable `Fix` snapshot | ✓ VERIFIED | `isUpdated()` checked before any consuming accessor (mandatory ordering honored); read-only UART use (no write/print to `serial_`); `fix_quality` correctly normalized from ASCII digit to 0-8 int (CR-01 fix present at line 68-70). |
| `firmware/gps_tracker/src/fix_gate.h/.cpp` | Pure 8-check accept/reject decision, no I/O | ✓ VERIFIED | No `Serial`/`HardwareSerial`/parser reference; uses `TinyGPSPlus::distanceBetween` (not hand-rolled); `reset()` implemented and now called on simulator transitions (CR-02 fix, see key links). |
| `firmware/gps_tracker/src/cadence.h/.cpp` | Hysteresis+debounce state machine, pacing decision | ✓ VERIFIED | No I/O; decides only from `speed_kmh`/`mono_ms`; initial state `STATIONARY`; debounce/hysteresis logic matches spec exactly. |
| `firmware/gps_tracker/BENCH.md` | Bench verification procedure + calibration record | ✓ VERIFIED | All 6 required sections present (Montagem, Build e flash, Comandos da serial, Formato das linhas, Roteiro de verificação, Registro de calibração); calibration table intentionally has blank observed-value cells (by design, not a stub — operator fills after real field testing). |
| `firmware/gps_tracker/src/main.cpp` | Bench harness wiring reader→gate→cadence→serial+LED | ✓ VERIFIED | No direct TinyGPSPlus accessor calls (only a comment mentions the library); `handleAccept()` is the single `onGatedFix` call site, reached only from `GateResult::ACCEPT` branches on both real and simulated paths; LED re-sourced from gate/cadence result. |

### Key Link Verification

| From | To | Via | Status | Details |
|------|-----|-----|--------|---------|
| `gps_reader.cpp` | `TinyGPSPlus` parser | `isUpdated()`-gated single read | ✓ WIRED | Ordering verified correct (isUpdated() checked before FixQuality()/lat()/lng()); no re-parsing of raw bytes elsewhere. |
| `main.cpp` (`loop()`, `simTick()`) | `fix_gate.cpp` (`FixGate::evaluate`) | Every snapshot gated before any other use | ✓ WIRED | Both entry points call `fixGate.evaluate(...)` before printing/forwarding; result stored and switched on. |
| `fix_gate.cpp` (`GateResult::ACCEPT`) | `cadence.cpp` (`Cadence::onGatedFix`) | Single call site inside `handleAccept()`, itself only reached from `ACCEPT` branches | ✓ WIRED | `grep -c 'onGatedFix' src/main.cpp` = 1 (both plans' explicit acceptance criterion); `grep -n -A6 'GateResult::ACCEPT'` shows the branch structure holds. |
| `main.cpp` (`simToggle()`) | `fix_gate.cpp` (`FixGate::reset()`) | Called on every simulator on/off transition | ✓ WIRED | CR-02 fix (commit `a63bead`) confirmed present at `main.cpp:193`; resolves the permanent-outlier-lockout bug the code review found. Runtime confirmation of the fix is still a human-verification item (see above) since it wasn't re-tested on hardware after the fix. |
| `main.cpp` (real vs. `[SIM]` reject dedup) | `printRejectIfNew()` | Per-origin dedup state (`originIndex()`) | ✓ WIRED | WR-04 fix confirmed present (2-element arrays keyed by origin). |

### Behavioral Spot-Checks

| Behavior | Command | Result | Status |
|----------|---------|--------|--------|
| `esp32-c3-supermini` build | `pio run -e esp32-c3-supermini` | SUCCESS, 5.15s, no new warnings, Flash 21.6%/RAM 4.5% | ✓ PASS |
| `esp32-devkit` build | `pio run -e esp32-devkit` | SUCCESS, 8.50s, no new warnings, Flash 21.7%/RAM 6.7% | ✓ PASS |
| Firmware runtime behavior (fixes, gating, cadence, simulator) | — | — | ? SKIP — no runnable entry point without flashing physical hardware; routed to human verification per this session's environment notes. |

### Probe Execution

No `scripts/*/tests/probe-*.sh` conventions or phase-declared probes exist for this project (embedded firmware, hardware-validated per CLAUDE.md — "No automated test suite"). Step 7c: SKIPPED (no probes found).

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|--------------|------------|--------------|--------|----------|
| GPS-01 | 01-01-PLAN.md | Node lê NEO-6M/M8N via UART e extrai lat/lon/velocidade/altitude/curso + timestamp UTC | ✓ SATISFIED (structurally; live confirmation pending) | `Fix` struct + `GpsReader::poll()` populate all fields; printed on serial. REQUIREMENTS.md marks Complete. |
| GPS-02 | 01-01-PLAN.md | Fixes passam por gating de qualidade — fix ruim nunca entra no log nem alimenta a cadência | ✓ SATISFIED (structurally; live confirmation pending) | `FixGate` 8-check pipeline; single `ACCEPT`-gated hand-off verified structurally. REQUIREMENTS.md marks Complete. |
| GPS-03 | 01-02-PLAN.md | Cadência adaptativa: ponto a cada ~5-10s em movimento, pausa quando parado | ✓ SATISFIED (structurally; live confirmation pending) | `Cadence` hysteresis+debounce state machine, 8s default interval, `DEFAULT_GPS_STATIONARY_HEARTBEAT_S=0` fully suppresses while parked. REQUIREMENTS.md marks Complete. |

No orphaned requirements: REQUIREMENTS.md's Phase 1 mapping (GPS-01/02/03) exactly matches what both plans declare in `requirements:` frontmatter.

### Anti-Patterns Found

None. Scanned all 6 created/modified source files (`gps_config.h`, `gps_types.h`, `gps_reader.{h,cpp}`, `fix_gate.{h,cpp}`, `cadence.{h,cpp}`, `main.cpp`) plus `BENCH.md` for `TODO|FIXME|XXX|TBD|HACK|PLACEHOLDER` and "not yet implemented"/"coming soon" — the only regex hits were the Portuguese word "Todo" ("every"/"all") in BENCH.md, a false positive, not a debt marker. No empty implementations, no hardcoded stub returns, no `console.log`-only handlers (n/a for this embedded target).

01-REVIEW.md's code review (2 critical + 4 warning findings) was re-checked against current source: CR-01 (ASCII fix-quality normalization), CR-02/WR-02 (FixGate reset + simulator continuity anchor), WR-01 (edge-gated health counters), WR-04 (per-origin reject dedup) are all confirmed present in the code as committed. WR-03 (loop-stall fix coalescing) was deliberately skipped by the reviewer's own assessment ("not necessarily worth fixing... given current loop() is lightweight") and remains an open, low-severity, non-blocking observability gap — not a must-have and not a debt marker in source, so it does not affect this phase's status.

## Human Verification Required

See the `human_verification` and `behavior_unverified_items` frontmatter above for the full list with concrete steps. In summary:

1. **MVP mode guard discrepancy** — ROADMAP Phase 1 goal is not in canonical User Story form despite `Mode: mvp`; needs a human decision (`/gsd mvp-phase 1` or explicit acceptance of standard verification).
2. **Full BENCH.md bench walkthrough on both boards** — the 9-step "Roteiro de verificação da Fase 1" (cold boot, warmup, first accept + antenna-cover staleness, injected rejections, stationary suppression, moving transition + ~8s cadence, moving→stationary transition, two-module confirmation) has not been run against the current (post-code-review-fixes) binary.
3. **CR-02/WR-02 regression check** — specifically re-verify that toggling the bench simulator off after a movement session does not leave real fixes permanently locked out by `REJECT_OUTLIER_JUMP`, since this was a critical bug found and fixed in this phase's code review but not re-tested on hardware afterward.

## Gaps Summary

No gaps found. All required artifacts exist, are substantive (no stubs), and are correctly wired per source-level review; both PlatformIO environments build clean; all three requirement IDs (GPS-01, GPS-02, GPS-03) are structurally satisfied and requirement coverage is complete with no orphans; the phase's own code review found and fixed its two critical bugs (verified present in source). The phase cannot reach a `passed` verdict this session only because its observable truths are inherently runtime/hardware behaviors (per this session's environment notes, live serial/timed observation cannot be auto-verified) — this is expected for a bench-validated embedded firmware phase and routes to `human_needed`, not `gaps_found`.

---

*Verified: 2026-08-01T21:52:08Z*
*Verifier: Claude (gsd-verifier)*
