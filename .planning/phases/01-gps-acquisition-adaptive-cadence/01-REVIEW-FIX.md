---
phase: 01-gps-acquisition-adaptive-cadence
fixed_at: 2026-08-01T00:00:00Z
review_path: .planning/phases/01-gps-acquisition-adaptive-cadence/01-REVIEW.md
iteration: 1
findings_in_scope: 6
fixed: 5
skipped: 1
status: partial
---

# Phase 1: Code Review Fix Report

**Fixed at:** 2026-08-01
**Source review:** .planning/phases/01-gps-acquisition-adaptive-cadence/01-REVIEW.md
**Iteration:** 1

**Summary:**
- Findings in scope: 6 (2 Critical + 4 Warning; Info findings excluded from scope)
- Fixed: 5 (CR-01, CR-02, WR-01, WR-02, WR-04)
- Skipped: 1 (WR-03)

## Fixed Issues

### CR-01: `fix_quality` stores ASCII digit codes, not the documented 0–8 integer codes

**Files modified:** `firmware/gps_tracker/src/gps_reader.cpp`
**Commit:** `e1d2f13`
**Applied fix:** Normalized `TinyGPSLocation::Quality`'s ASCII-digit enumerator to the documented 0-8 integer range by subtracting `'0'` before storing into `out.fix_quality`, exactly as the review's suggested fix specified. Verified the surrounding code (isValid()-before-accessor ordering, HDOP sentinel) was unaffected.

### CR-02: `FixGate`'s last-accepted-position reference is never reset around simulator sessions — permanent outlier-jump lockout

**Files modified:** `firmware/gps_tracker/src/main.cpp`
**Commit:** `a63bead`
**Applied fix:** Added `fixGate.reset()` at the top of `simToggle()` so the shared `FixGate` instance's last-accepted-position reference is reconciled on every real/simulated source transition, preventing the permanent `REJECT_OUTLIER_JUMP` lockout the review identified. Also introduced a dedicated `lastAcceptedFix`/`haveAcceptedFix` pair (populated only inside `handleAccept()`) and switched the simulator's first-activation anchor to read from it instead of the raw `lastFix`, which also resolves WR-02 (same root cause, same fix).

### WR-01: `[HEALTH]` reject counters are inflated by loop-rate re-evaluation, not per-fix events

**Files modified:** `firmware/gps_tracker/src/main.cpp`
**Commit:** `f7f59ad`
**Applied fix:** Changed `printRejectIfNew()` to return `true` only when it actually printed a new line (i.e. the reason changed since the last call for that origin). The cold-boot/stale watchdog block in `loop()` (the block the review specifically flagged, not the once-per-actual-Fix reject path elsewhere in `loop()`, which was already correct) now increments `gateCounters[REJECT_NO_FIX]`/`gateCounters[REJECT_STALE]` only on that edge signal instead of unconditionally every iteration.

### WR-02: Simulator's first-activation anchor can be poisoned by a rejected raw fix

**Files modified:** `firmware/gps_tracker/src/main.cpp`
**Commit:** `a63bead` (same commit as CR-02 — identical root cause and fix suggestion)
**Applied fix:** See CR-02 above; the `lastAcceptedFix` tracking introduced there is exactly this finding's suggested fix.

### WR-04: Reject-line dedup state is shared across real and `[SIM]`-origin fixes

**Files modified:** `firmware/gps_tracker/src/main.cpp`
**Commit:** `4374993`
**Applied fix:** Converted `lastPrintedReason`/`havePrintedReason` from single globals into 2-element arrays keyed by a new `originIndex()` helper (`0` = real fixes, `1` = `[SIM] `-origin fixes), so a repeated reject reason on one source can no longer silently suppress the first occurrence of that reason on the other source. Updated `handleAccept()`'s reset of `havePrintedReason` to target only the calling origin's slot.

## Skipped Issues

### WR-03: `GpsReader::poll()` can silently coalesce multiple fixes into one if `loop()` stalls

**File:** `firmware/gps_tracker/src/gps_reader.cpp:39-53`
**Reason:** The review's own Fix note already deems this non-essential for Phase 1 ("Not necessarily worth fixing in Phase 1 given current loop() is lightweight"). A technically-correct implementation of the suggested `coalescedFixes_` counter requires distinguishing a *location-specific* sentence commit from any other NMEA sentence type — TinyGPSPlus's public `encode()` API only reports "a sentence with valid checksum completed" (true for GGA/RMC/GSA/GSV/VTG alike), not which internal object it updated. A naive "count sentences per `poll()` call" proxy would increment on essentially every normal `poll()` call (since a GPS module emits multiple sentence types within the same ~1 Hz cycle), producing a misleading rather than informative counter. Deferred rather than force an imprecise fix into production code; revisit if field data ever shows `loop()` stalling near the 1 Hz fix cycle.
**Original issue:** `poll()` drains the entire UART backlog before checking `location.isUpdated()` once; if `loop()` stalls past one fix cycle, an intermediate fix (and its `mono_ms`/`age_ms` pairing) can be silently dropped with no counter or log evidence.

---

_Fixed: 2026-08-01_
_Fixer: Claude (gsd-code-fixer)_
_Iteration: 1_
