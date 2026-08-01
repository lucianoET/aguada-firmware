---
phase: 01-gps-acquisition-adaptive-cadence
reviewed: 2026-08-01T00:00:00Z
depth: standard
files_reviewed: 10
files_reviewed_list:
  - firmware/gps_tracker/include/gps_config.h
  - firmware/gps_tracker/include/gps_types.h
  - firmware/gps_tracker/src/gps_reader.h
  - firmware/gps_tracker/src/gps_reader.cpp
  - firmware/gps_tracker/src/fix_gate.h
  - firmware/gps_tracker/src/fix_gate.cpp
  - firmware/gps_tracker/src/cadence.h
  - firmware/gps_tracker/src/cadence.cpp
  - firmware/gps_tracker/src/main.cpp
  - firmware/gps_tracker/platformio.ini
findings:
  critical: 2
  warning: 4
  info: 4
  total: 10
status: issues_found
---

# Phase 1: Code Review Report

**Reviewed:** 2026-08-01
**Depth:** standard
**Files Reviewed:** 10
**Status:** issues_found

## Summary

The module split (`gps_reader` / `fix_gate` / `cadence` / `main.cpp`) faithfully implements the structural guarantees the plans call for: `gps_reader` is the sole UART/parser owner, `fix_gate` is a pure decision function over the `Fix` snapshot with no I/O, `cadence` decides only from `speed_kmh`/`mono_ms`, and `onGatedFix`/downstream hand-off both live exclusively inside the single `GateResult::ACCEPT` branch. The `isUpdated()`-before-any-consuming-accessor ordering in `GpsReader::poll()` is correct, the staleness check uses `age_ms` (not `isValid()` alone) as required, `millis()`-delta arithmetic throughout uses the safe unsigned-subtraction form (overflow-safe across the ~49.7-day wrap), and the outlier-jump check correctly uses `TinyGPSPlus::distanceBetween()` rather than a hand-rolled haversine.

However, two bugs were found that are serious enough to block: (1) the `fix_quality` field is populated directly from `TinyGPSLocation::Quality`, an enum whose underlying values are ASCII digit characters (`'0'`=48 … `'8'`=56), not the small integers 0–8 the header comment, the gate logic, and every printed `q=` field assume; and (2) `FixGate`'s internal last-accepted-position reference is never reset when the bench simulator is toggled on or off, which — given the shared `FixGate` instance the real path and the simulator both feed — produces a permanent `REJECT_OUTLIER_JUMP` lockout the moment real fixes resume after any simulator session that moved position, exactly the sequence Plan 02's own human-check step 5 instructs the operator to perform.

Beyond those two, there are several latent-defect/fragility findings (a health-counter inflation bug, a bad simulator anchor on first activation, backlog coalescing in `poll()`, and shared reject-dedup state across real/simulated origins) plus a handful of low-severity style/documentation nits.

## Critical Issues

### CR-01: `fix_quality` stores ASCII digit codes, not the documented 0–8 integer codes

**File:** `firmware/gps_tracker/src/gps_reader.cpp:65`
**Issue:**
```cpp
out.fix_quality = gps_.location.isValid() ? static_cast<uint8_t>(gps_.location.FixQuality()) : 0;
```
`TinyGPSLocation::Quality` is declared as:
```cpp
enum Quality { Invalid = '0', GPS = '1', DGPS = '2', PPS = '3', RTK = '4', FloatRTK = '5', Estimated = '6', Manual = '7', Simulated = '8' };
```
(`.pio/libdeps/*/TinyGPSPlus/src/TinyGPS++.h:57`) — its enumerators are the raw ASCII bytes of the GGA field-6 character, i.e. `'1'` = 49, `'2'` = 50, etc. Casting that straight to `uint8_t` stores 49–56, not 1–8.

This breaks the documented contract in `gps_types.h:17` (`// GGA field 6 (TinyGPSLocation::Quality), 0 = invalid`) and the comment in `fix_gate.cpp:26-28` (`"Fix quality 1 (standalone GPS) is the accept floor"`). `FixGate::evaluate()`'s `f.fix_quality < 1` check still happens to work today only because the *only* two values that ever reach it in practice are `0` (never-fixed) and `49+` (any real fix quality) — but every `q=` field the bench operator sees on `[FIX]`/`[ACCEPT]`/`[REJECT]`/`[EMIT]`/`[SIM]` lines will read `q=49` instead of `q=1`, contradicting the plan's own documentation and BENCH.md's field-format table. Worse, this is a landmine: Open Question 2 in `01-RESEARCH.md` explicitly flags "should GGA fix-quality be stricter (>= 2)?" as something to revisit — the moment anyone changes the gate to `f.fix_quality >= 2` (a perfectly reasonable literal reading of the documented semantics), it will silently reject every fix from both target modules, because the real values are 50+, never 2.

**Fix:**
```cpp
// TinyGPSLocation::Quality's enumerators are ASCII digit characters
// ('0'..'8'), not small integers — normalize before storing.
out.fix_quality = gps_.location.isValid()
    ? static_cast<uint8_t>(gps_.location.FixQuality() - '0')
    : 0;
```

### CR-02: `FixGate`'s last-accepted-position reference is never reset around simulator sessions — permanent outlier-jump lockout

**File:** `firmware/gps_tracker/src/main.cpp` (simulator block, `simToggle()` ~L158-180) and `firmware/gps_tracker/src/fix_gate.h`/`fix_gate.cpp` (`reset()` declared, never called anywhere in the codebase)
**Issue:** `main.cpp` uses a single, shared `static FixGate fixGate;` instance for both real GPS fixes (`loop()`) and the bench simulator (`simTick()`). `FixGate::evaluate()`'s outlier-jump check (step 8) compares every incoming fix against `lastLat_`/`lastLon_`, which is only updated on `ACCEPT` and is *deliberately* left untouched on `REJECT_OUTLIER_JUMP` ("one wild sample cannot poison the reference point" — by design, and correct in isolation).

The problem is the transition between real and simulated data sources:
- While the simulator runs (`s`), any real accepted position moves the shared reference; conversely, once the simulator itself gets its first `ACCEPT`, `lastLat_`/`lastLon_` becomes the simulator's synthetic position, which — after even a short "moving" test run (Task 1 human-check step 2, or Task 2 step 2) — will have drifted tens to hundreds of metres away from the bench's real GPS coordinates.
- The distance/time outlier check uses `f.mono_ms` (wall-clock `millis()`, continuous across the sim/real switch) as the elapsed-time denominator, so it correctly reflects real elapsed time — meaning the *first real fix* that arrives after `s` is pressed to turn the simulator off computes distance from the simulator's drifted position over only ~1 second of elapsed time, producing an easily-exceeded implied speed and `REJECT_OUTLIER_JUMP`.
- Because outlier rejections never update `lastLat_`/`lastLon_`, **every subsequent real fix is rejected the same way, forever**, since the stale reference point never advances. There is no code path anywhere that calls `FixGate::reset()` to recover — the only way out is a full device reboot.

This directly contradicts Plan 02 Task 2's own documented human-check step: *"5. Press `s` to stop the simulator and confirm real fixes resume driving the pipeline."* As implemented, that step will fail after almost any simulator session that involved movement (which steps 2-4 explicitly require). The inverse direction has a milder version of the same bug: `simToggle()` (main.cpp ~L160-172) anchors the simulator's start position from `lastFix` (the last *raw* polled `Fix`, which may itself have failed the gate — e.g. a null-island or bad-HDOP raw sample) rather than from `FixGate`'s true last-*accepted* reference, which can produce a bogus `REJECT_OUTLIER_JUMP` on the very first simulated tick too.

**Fix:** Reset (or otherwise reconcile) `FixGate`'s continuity state on every simulator-mode transition, e.g.:
```cpp
// in simToggle(), both branches:
static void simToggle() {
    simActive_ = !simActive_;
    fixGate.reset();   // the two sources must not share a stale
                        // last-accepted reference across the switch
    ...
}
```
and anchor `simLat_`/`simLon_` from a tracked "last *accepted* Fix" (e.g. store `lastFix` only inside `handleAccept()`, not on every raw poll) rather than the last raw `Fix`, so a rejected raw sample can never seed the simulator's continuity point.

## Warnings

### WR-01: `[HEALTH]` reject counters are inflated by loop-rate re-evaluation, not per-fix events

**File:** `firmware/gps_tracker/src/main.cpp:338-347`
**Issue:** The "no new snapshot" branch of `loop()` re-runs on *every* `loop()` iteration where `poll()` returned `false` — which, at an unthrottled ESP32 `loop()` rate, is thousands of iterations per second, not once per lost/never-acquired fix:
```cpp
if (!everAccepted) {
    gateCounters[static_cast<uint8_t>(GateResult::REJECT_NO_FIX)]++;
    ...
} else if ((millis() - lastAcceptedAtMs) >= DEFAULT_GPS_FRESH_MS) {
    gateCounters[static_cast<uint8_t>(GateResult::REJECT_STALE)]++;
    ...
}
```
`printRejectIfNew()` dedups the *printed* line, but `gateCounters[...]` is incremented unconditionally every iteration, regardless of whether anything new happened. As a result `no_fix=`/`stale=` in `[HEALTH]` will be many orders of magnitude larger than `accept=`/`hdop_rej=`/`sats_rej=`/`warmup=`/`jump=` (which only increment once per actual ~1 Hz `Fix`), making the counters meaningless for comparison and, under a sustained no-fix condition (e.g. antenna disconnected for days while the tracker sits in storage), capable of wrapping a `uint32_t` counter within the field-deployment lifetime of the device.

**Fix:** Gate the watchdog increment on an edge/interval, e.g. only increment when the printed line actually changes (reuse the `printRejectIfNew` dedup signal), or drive the watchdog from a periodic timer instead of every `loop()` pass:
```cpp
bool isNewReason = !havePrintedReason || result != lastPrintedReason;
if (isNewReason) gateCounters[...]++;
```

### WR-02: Simulator's first-activation anchor can be poisoned by a rejected raw fix

**File:** `firmware/gps_tracker/src/main.cpp:160-172` (`simToggle()`)
**Issue:** See CR-02 — `simLat_`/`simLon_` are seeded from `lastFix` (last raw `poll()` result) rather than from a tracked last-*accepted* position. If the most recent real fix before the simulator is turned on failed the gate (e.g. `REJECT_NULL_ISLAND`, `REJECT_HDOP`, or a rejected outlier), the simulator starts from that bad position, and its very first synthetic tick can itself trigger a spurious `REJECT_OUTLIER_JUMP` against `FixGate`'s true (older, good) last-accepted reference — undermining the "prove it from a desk" workflow Task 2 exists to deliver.
**Fix:** Track a dedicated `lastAcceptedFix` (set only inside `handleAccept()`) and anchor the simulator from that instead of the raw `lastFix`.

### WR-03: `GpsReader::poll()` can silently coalesce multiple fixes into one if `loop()` stalls

**File:** `firmware/gps_tracker/src/gps_reader.cpp:39-53`
**Issue:** `poll()` drains the *entire* UART receive backlog in a `while (serial_.available())` loop before checking `location.isUpdated()` once. Under normal ~1 Hz NMEA timing with a tight `loop()` this is harmless, but if `loop()` is ever delayed for more than one fix cycle (e.g. a long `Serial.printf()` burst, or future work added to `loop()`), more than one commit can land in the same `poll()` call. Only the last-committed values are ever read back, so an intermediate fix (and, in principle, an intermediate `mono_ms`/`age_ms` pairing the outlier-jump math relies on) is silently dropped with no counter or log evidence it happened — in tension with the project's stated core value ("nenhum ponto do trajeto se perde").
**Fix:** Not necessarily worth fixing in Phase 1 given current loop() is lightweight, but worth a counter (e.g. increment a `coalescedFixes_` stat whenever more than one full sentence commits within a single `poll()` call) so the condition is at least observable in `[HEALTH]` if it ever occurs in the field.

### WR-04: Reject-line dedup state is shared across real and `[SIM]`-origin fixes

**File:** `firmware/gps_tracker/src/main.cpp:34-56` (`lastPrintedReason`/`havePrintedReason`, `printRejectIfNew`)
**Issue:** `lastPrintedReason`/`havePrintedReason` are single global variables consulted by both the real-fix reject path and the simulator's reject path. If the last real reject was, say, `REJECT_STALE`, and the operator then turns on the simulator and injects a fix that also evaluates to `REJECT_STALE` (unlikely for the simulator specifically, but generally: any reason recurrence across a source switch), the `[SIM]`-tagged line will be silently suppressed because the dedup key doesn't include the origin — a captured log can then show a real rejection immediately followed by simulator activity with no visible `[SIM] [REJECT]` line at all, even though BENCH.md's whole premise is that every simulator-triggered rejection is independently observable.
**Fix:** Key the dedup on `(origin, result)` rather than `result` alone, e.g. two separate `lastPrintedReason`/`havePrintedReason` pairs (or fold `origin` into the comparison).

## Info

### IN-01: Outlier-jump check is skipped entirely when `mono_ms` delta is zero

**File:** `firmware/gps_tracker/src/fix_gate.cpp:78-86`
**Issue:** `if (dtHours > 0.0)` guards the whole outlier check; if two gate-eligible fixes arrive with the exact same `mono_ms` (possible only in pathological cases — e.g. `poll()` coalescing two commits into the same `millis()` tick, see WR-03), the jump check is bypassed rather than treated as "instantaneous teleport ⇒ reject." Very low practical likelihood at 1 Hz NMEA but worth a one-line comment or an explicit `dtHours <= 0.0` reject if `WR-03` is ever addressed.

### IN-02: `platformio.ini`'s `-I../shared` include path is unused by any `gps_tracker` source

**File:** `firmware/gps_tracker/platformio.ini:17,41`
**Issue:** Both envs add `-I../shared` (pointing at `firmware/shared/protocol.h`, the Aguada v3 reservoir-telemetry protocol header), but no file under `firmware/gps_tracker/src` or `include` references it — this GPS tracker is an explicitly protocol-v3-agnostic standalone project per `.claude/CLAUDE.md` ("compatibilidade: não quebrar protocolo v3... extensão de pacote deve conviver com os 16 bytes atuais"). Pre-existing from the walking-skeleton commit rather than introduced by this phase, but worth removing as dead configuration since it invites confusion about whether this firmware depends on the reservoir protocol.

### IN-03: `%u` format specifier used with `uint8_t` arguments throughout `main.cpp`

**File:** `firmware/gps_tracker/src/main.cpp` (`printEmit`, `printRejectIfNew`, the `[HEALTH]` `Serial.printf`, e.g. lines 52-53, 61-64, 363-384)
**Issue:** `f.sats`/`f.fix_quality`/`lastFix.sats` are `uint8_t`, passed to `%u` (which expects `unsigned int`). The default argument promotions make this work correctly in practice on every mainstream ABI (including ESP32/GCC), but it is technically a format/argument-type mismatch that stricter `-Wformat` settings would flag. Low priority; consider `static_cast<unsigned>(...)` at call sites if `-Wformat=2` is ever enabled.

### IN-04: Cold-boot `[REJECT]` watchdog line prints a misleading `hdop=0.0`

**File:** `firmware/gps_tracker/src/main.cpp:340`
**Issue:** Before the first `poll()` ever returns a snapshot, `lastFix` is a default-constructed `Fix{}`, so `lastFix.hdop` is `0.0` — not the `99.9f` "unavailable" sentinel `GpsReader::poll()` uses for a genuinely-absent HDOP. The very first `[REJECT] reason=REJECT_NO_FIX ... hdop=0.0 ...` line therefore looks like a suspiciously *good* HDOP reading rather than "no data yet." Cosmetic only; consider seeding `lastFix.hdop = 99.9f` at declaration for consistency with the rest of the codebase's sentinel convention.

---

_Reviewed: 2026-08-01_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
