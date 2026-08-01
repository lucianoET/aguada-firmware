# Phase 1: GPS Acquisition & Adaptive Cadence - Research

**Researched:** 2026-08-01
**Domain:** Embedded NMEA GPS parsing (TinyGPSPlus) + fix-quality gating + speed-based adaptive cadence, ESP32 Arduino/PlatformIO
**Confidence:** MEDIUM — library API behavior is HIGH (verified by reading the actual vendored TinyGPSPlus source in this repo's `.pio/libdeps/`, and by the already-hardware-validated bench skeleton); concrete numeric thresholds (HDOP, satellite count, speed/hysteresis values) are LOW-MEDIUM (general GNSS engineering practice + web search, not validated against this specific hardware/mount in the field yet)

## Summary

Phase 1's job is narrow and foundational: turn raw NMEA characters from the ATGM336H/NEO-6M into a trustworthy, well-paced stream of `Fix` events that Phase 2 (flash logger) can consume without re-deriving any of this logic. The bench walking skeleton (`firmware/gps_tracker/src/main.cpp`, hardware-validated 2026-08-01) already proves the wiring and the parsing library work — TinyGPSPlus resolved to **1.1.0** (newer than the milestone-level research's `1.0.3a` pin, still satisfies the `^1.0.3` constraint) and got a real fix on an ESP32-C3 with a NEO-6M.

The single most important fact this research surfaced by reading TinyGPSPlus's source directly (not assumed from tutorials): **`gps.location.isValid()` is a one-way latch — it becomes `true` the first time any sentence reports a fix, and is *never* reset to `false` again**, even after the receiver loses the fix entirely. There is no "fix lost" signal from `isValid()`. The only way to detect a stale/lost fix is `gps.location.age()` growing past a freshness threshold. This directly confirms and sharpens Pitfall 4 from the milestone `PITFALLS.md` and must be the first thing the `fix_gate` module gets right — a gate that only checks `isValid()` will happily accept a fix that is minutes old and wildly wrong.

The rest of the gating stack is standard NMEA/GNSS practice: HDOP from GGA field 8 (TinyGPSPlus's `hdop.hdop()` already returns the real decimal value, e.g. `1.2`, not a scaled integer), satellite count from GGA field 7 (satellites *used*, not in view), and fix quality from GGA field 6 (`0`=invalid, `1`=GPS, `2`=DGPS...). GSA-based 2D/3D fix-type is **not** parsed by core TinyGPSPlus and is not needed for this phase — GGA fix-quality + HDOP + satellite count is sufficient and keeps the implementation minimal, consistent with the "NMEA-agnostic, no proprietary config" decision (D-02).

For cadence, GPS-reported speed (RMC field 7, exposed as `gps.speed.kmph()`) is the correct movement signal — not position-delta — because it comes from the receiver's own Doppler-derived velocity solution and is far more stable than differencing two noisy lat/lon fixes. The bench session already observed ~2.6 km/h of speed jitter while stationary; any moving/stationary threshold must sit safely above that noise floor and use a debounced hysteresis band, not a single crossing point, to avoid state-flapping (Pitfall 7).

**Primary recommendation:** Build three small, testable modules — `gps_reader` (owns UART + TinyGPSPlus instance, exposes a `Fix` snapshot struct), `fix_gate` (pure function: `Fix` + current TinyGPSPlus state → accept/reject + reason), `cadence` (owns moving/stationary state + last-accepted timestamp, decides whether a gated fix is emitted downstream) — wired together in `main.cpp`. Keep all thresholds as named constants (`#define DEFAULT_*` in `platformio.ini` `build_flags`, matching repo convention) so Phase 2+ and later field tuning don't require touching gate/cadence logic.

## Architectural Responsibility Map

This phase is single-firmware, no client/server split yet (that begins in Phase 3/4). Tiers below are the firmware's internal layers, not network tiers.

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| NMEA byte stream → parsed fields (lat/lon/speed/course/alt/UTC) | Firmware: `gps_reader` module (TinyGPSPlus wrapper) | GPS module hardware (source of raw NMEA) | TinyGPSPlus does the character-streaming parse; `gps_reader` just owns the UART lifecycle and exposes a `Fix` struct — no gating logic here |
| Fix-quality gating (validity, freshness/age, HDOP, satellite count, outlier-speed check) | Firmware: `fix_gate` module | — | Pure decision function over already-parsed TinyGPSPlus state; must run before *anything* else touches a fix, including cadence |
| Adaptive cadence (moving vs. stationary pacing, debounce/hysteresis) | Firmware: `cadence` module | `fix_gate` (only ever receives gated-accepted fixes) | Cadence must never evaluate a fix that hasn't passed the gate — an ungated jitter spike could otherwise flip moving/stationary state |
| Bench observability (serial print of raw NMEA + parsed summary) | Firmware: `main.cpp` (temporary bench harness) | — | Debug-only; not a boundary any other module depends on, and is expected to be replaced/extended once Phase 2's flash logger becomes the real downstream consumer |
| Downstream consumption of accepted fixes | Out of scope this phase (Phase 2: flash logger) | — | Phase 1's only contract with Phase 2 is: expose a clean `Fix` struct + an "accepted" event/callback; do not couple to how Phase 2 stores it |

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| GPS-01 | Node lê NEO-6M/M8N via UART e extrai lat/lon/velocidade/altitude/curso + timestamp UTC (TinyGPSPlus) | `gps_reader` module design (Architecture Patterns §1); TinyGPSPlus API table (Code Examples); bench skeleton already proves UART wiring + parsing on both target boards |
| GPS-02 | Fixes passam por gating de qualidade (validade, HDOP, sats) — fix ruim nunca entra no log nem alimenta a cadência | `fix_gate` module design (Architecture Patterns §2); TinyGPSPlus `isValid()`/`age()`/`hdop()`/`satellites` semantics (Common Pitfalls §1); recommended thresholds table |
| GPS-03 | Cadência adaptativa: ponto a cada ~5–10 s em movimento, pausa quando parado (classificação por velocidade GPS abaixo de limiar por N segundos) | `cadence` module design (Architecture Patterns §3); speed-based debounce/hysteresis recommendation (Common Pitfalls §3); bench-observed 2.6 km/h stationary jitter as the noise-floor baseline |

</phase_requirements>

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| **TinyGPSPlus** | `1.1.0` (already resolved in `firmware/gps_tracker/.pio/libdeps/*/TinyGPSPlus/library.json`, satisfies the existing `^1.0.3` pin in `platformio.ini`) | NMEA `GGA`+`RMC` parsing → lat/lon/speed/course/altitude/date/time/hdop/satellites | `[VERIFIED: repo build artifact]` — this is not a recommendation, it's already the working, hardware-validated dependency for this project. Confirmed by reading the vendored `library.json`/`.properties` directly (author: Mikal Hart, repo `github.com/mikalhart/TinyGPSPlus`). |

No new libraries are needed for Phase 1 — `gps_reader`, `fix_gate`, and `cadence` are all pure application code on top of the already-installed TinyGPSPlus + Arduino core (`HardwareSerial`, `millis()`).

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| *(none)* | — | — | Phase 1 has no supporting-library needs. Flash (LittleFS), MQTT (PubSubClient), WiFi (WiFiMulti) are Phase 2/3 concerns per the milestone `STACK.md` and out of this phase's boundary per `01-CONTEXT.md`. |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| TinyGPSPlus | Hand-rolled GSA parsing via `TinyGPSCustom` for 2D/3D `fixType` | Only worth adding if GGA fix-quality (`0`/`1`/`2`) + HDOP + satellite count prove insufficient in field testing. Core library already covers everything GPS-02 requires without this. |
| TinyGPSPlus | NeoGPS | Not worth switching per milestone `STACK.md` — no RAM/CPU constraint exists on ESP32 DevKit classic (D-01 chose the board with the *most* headroom specifically for this reason). |

**Installation:** No change needed — `firmware/gps_tracker/platformio.ini` already pins `mikalhart/TinyGPSPlus @ ^1.0.3` for both `esp32-devkit` and `esp32-c3-supermini` envs.

**Version verification:** `TinyGPSPlus` resolved version confirmed via direct read of `firmware/gps_tracker/.pio/libdeps/esp32-c3-supermini/TinyGPSPlus/library.json` → `"version": "1.1.0"`. This is a primary-source, on-disk confirmation (stronger than a registry lookup) since it reflects what the bench build actually compiled against.

## Package Legitimacy Audit

The `package-legitimacy check` seam does not support the PlatformIO/Arduino Library Registry ecosystem (only `npm|pypi|crates`) — invoked and confirmed via CLI usage error. Verification below is manual, using primary sources instead.

| Package | Registry | Age | Downloads | Source Repo | Verdict | Disposition |
|---------|----------|-----|-----------|--------------|---------|-------------|
| TinyGPSPlus | PlatformIO Registry / Arduino Library Manager | Original project since 2008 (per license header in vendored source, "Copyright (C) 2008-2024 Mikal Hart") | N/A (registry-download-count not exposed by this seam) — but it is the de facto standard Arduino NMEA library, already depended on elsewhere in this codebase (milestone `STACK.md`) | `github.com/mikalhart/TinyGPSPlus` (confirmed in vendored `library.json`) | OK | Approved — already installed and hardware-validated in this exact project, no action needed |

**Packages removed due to [SLOP] verdict:** none
**Packages flagged as suspicious [SUS]:** none

*No new packages are introduced in this phase; TinyGPSPlus was already vetted at the milestone-research stage (`STACK.md`, Context7-sourced) and reconfirmed here by reading the actual resolved build artifact.*

## Architecture Patterns

### System Architecture Diagram

```
GPS module (ATGM336H or NEO-6M)
   │  NMEA @ 9600 baud, 1 sentence set/sec ($GNRMC/$GPRMC + $GNGGA/$GPGGA, +/- $..GSV/$..GSA ignored)
   ▼
HardwareSerial (UART2: GPIO16/17 on ESP32 DevKit; UART1: GPIO20/21 on C3 SuperMini)
   │  raw bytes, char-by-char
   ▼
gps_reader.encode(c) → TinyGPSPlus state machine
   │  on sentence checksum pass: commits location/speed/course/altitude/date/time/satellites/hdop
   ▼
fix_gate.evaluate(gps state) ────────────► REJECT (stale / low HDOP / too few sats / not-yet-warmed-up / outlier jump)
   │  ACCEPT only if: location.isValid() && location.age() < FRESH_MS
   │                  && hdop.isValid() && hdop.hdop() <= HDOP_MAX
   │                  && satellites.isValid() && satellites.value() >= SATS_MIN
   │                  && warmup fix-count satisfied
   │                  && implied speed vs last accepted fix <= MAX_PLAUSIBLE_KMH
   ▼
cadence.onGatedFix(Fix) → moving/stationary state machine (speed-based, debounced)
   │  MOVING: emit fix if (now - last_emitted) >= CADENCE_MOVING_S
   │  STATIONARY: suppress emission (optionally emit low-rate heartbeat)
   ▼
Emitted Fix{lat, lon, speed, course, alt, utc_ts} ──► (Phase 1: printed to USB serial)
                                                    ──► (Phase 2: flash logger, out of scope here)
```

A reader should be able to trace: NMEA byte in → TinyGPSPlus state → gate decision → cadence decision → emitted (or suppressed) fix, entirely inside the firmware, with no network/flash dependency for this phase.

### Recommended Project Structure

```
firmware/gps_tracker/
├── platformio.ini          # existing — esp32-devkit + esp32-c3-supermini envs, TinyGPSPlus pinned
├── gps_tracker_pinout.md   # existing — canonical pin reference
├── include/
│   └── gps_types.h         # Fix struct, GateResult enum, shared constants (thresholds as #define/build_flags)
└── src/
    ├── main.cpp            # boot, wiring gps_reader → fix_gate → cadence → serial output
    ├── gps_reader.cpp/.h   # owns HardwareSerial + TinyGPSPlus instance, exposes snapshot Fix + raw TinyGPSPlus refs
    ├── fix_gate.cpp/.h     # pure evaluate(TinyGPSPlus&, GateState&) → GateResult; owns warmup counter + last-accepted-fix for outlier check
    └── cadence.cpp/.h      # owns moving/stationary state + hysteresis debounce counters + last-emitted timestamp
```

### Structure Rationale

- **Three modules, each independently testable in isolation** (no hardware needed to unit-test `fix_gate`/`cadence` logic if fed synthetic TinyGPSPlus-shaped state) — mirrors the milestone `ARCHITECTURE.md`'s recommendation to isolate the highest-risk logic (there, the radio state machine; here, the gating/cadence decisions that every downstream feature trusts).
- **`gps_reader` never makes accept/reject decisions** — it only owns the UART and exposes state. This keeps the "don't hand-roll UART parsing" boundary clean and matches the existing repo pattern (`firmware/node/src/sensor_filter.*` separates raw sensor read from filtering decision).
- **`fix_gate` is a pure function over TinyGPSPlus's already-committed state**, not a re-parser — it reads `gps.location`, `gps.hdop`, `gps.satellites` directly rather than duplicating NMEA parsing.
- **`cadence` only ever sees gate-accepted fixes** — enforced by call order in `main.cpp` (`if (fix_gate.evaluate(...) == ACCEPT) cadence.onGatedFix(...)`), directly implementing the "fix-quality gating must exist before adaptive cadence" ordering from milestone `PITFALLS.md` Pitfall 4/7.

### Pattern 1: TinyGPSPlus staleness check (not `isValid()` alone)

**What:** `location.isValid()` is a one-way latch (see Summary) — it does not reset to `false` when the fix is lost. The only reliable "is this fix current" signal is `age()`.

**When to use:** Every single read of `gps.location`, and by extension `gps.speed`/`gps.course` (which commit alongside location only when `sentenceHasFix` is true — see TinyGPSPlus source).

**Example:**
```cpp
// Source: read directly from firmware/gps_tracker/.pio/libdeps/*/TinyGPSPlus/src/TinyGPS++.cpp (vendored, this repo)
// TinyGPSLocation::commit() sets valid=updated=true, but nothing in the library
// ever sets valid=false again on fix loss — confirmed by reading endOfTermHandler():
//   case GPS_SENTENCE_RMC: if (sentenceHasFix) { location.commit(); speed.commit(); course.commit(); }
//   case GPS_SENTENCE_GGA: if (sentenceHasFix) { location.commit(); altitude.commit(); }
//                          satellites.commit(); hdop.commit();  // these commit regardless of fix
//
// Correct freshness check:
bool isFreshFix(TinyGPSPlus &gps, uint32_t freshMs) {
    return gps.location.isValid() && gps.location.age() < freshMs;
    // age() returns ULONG_MAX if valid==false, so the isValid() check is still required
    // (age() alone on a never-fixed unit would otherwise be an undefined/huge number, not "stale")
}
```

### Pattern 2: fix_gate — multi-criteria accept/reject

**What:** A single evaluation function combining freshness, fix quality, HDOP, satellite count, a post-fix warmup counter, and an outlier/implausible-speed check — mirroring the existing repo's `sensor_filter.*` "outlier reject → threshold" philosophy (`AGUADA_SYSTEM_DOC.md` §6) applied to GPS instead of ultrasonic.

**When to use:** Called once per new committed fix, before cadence logic ever runs.

**Example:**
```cpp
// gps_types.h — thresholds as build_flags-overridable constants, matching repo convention
#ifndef GPS_HDOP_MAX
#define GPS_HDOP_MAX 5.0f          // reject above this HDOP (standard "moderate or worse" cutoff)
#endif
#ifndef GPS_SATS_MIN
#define GPS_SATS_MIN 4             // hard minimum for any 3D solution
#endif
#ifndef GPS_FRESH_MS
#define GPS_FRESH_MS 2500          // 1 fix/sec expected; allow ~2 missed cycles before "stale"
#endif
#ifndef GPS_WARMUP_FIXES
#define GPS_WARMUP_FIXES 3         // require N consecutive gate-passing fixes before trusting for cadence
#endif
#ifndef GPS_MAX_PLAUSIBLE_KMH
#define GPS_MAX_PLAUSIBLE_KMH 200.0f  // implied-speed outlier gate vs last accepted fix
#endif

enum class GateResult { ACCEPT, REJECT_STALE, REJECT_NO_FIX, REJECT_HDOP, REJECT_SATS, REJECT_WARMUP, REJECT_OUTLIER_JUMP };

GateResult FixGate::evaluate(TinyGPSPlus &gps) {
    if (!gps.location.isValid() || gps.location.age() >= GPS_FRESH_MS) return GateResult::REJECT_STALE;
    if (!(gps.location.FixQuality() >= TinyGPSLocation::Quality::GPS)) return GateResult::REJECT_NO_FIX; // GGA field 6 >= 1
    if (!gps.hdop.isValid() || gps.hdop.hdop() > GPS_HDOP_MAX) return GateResult::REJECT_HDOP;
    if (!gps.satellites.isValid() || gps.satellites.value() < GPS_SATS_MIN) return GateResult::REJECT_SATS;
    if (consecutivePassCount_ < GPS_WARMUP_FIXES) { consecutivePassCount_++; return GateResult::REJECT_WARMUP; }
    if (haveLastAccepted_) {
        double dtS = (millis() - lastAcceptedMs_) / 1000.0;
        double distKm = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(),
                                                        lastLat_, lastLng_) / 1000.0;
        if (dtS > 0 && (distKm / (dtS / 3600.0)) > GPS_MAX_PLAUSIBLE_KMH) return GateResult::REJECT_OUTLIER_JUMP;
    }
    lastLat_ = gps.location.lat(); lastLng_ = gps.location.lng();
    lastAcceptedMs_ = millis(); haveLastAccepted_ = true;
    return GateResult::ACCEPT;
}
```
Note: `gps.location.FixQuality()` (capital F) is the TinyGPSPlus accessor for the GGA fix-quality enum (`Invalid='0'`, `GPS='1'`, `DGPS='2'`, ...) — do not confuse with the lowercase-only fields; verified from `TinyGPS++.h` enum + accessor declarations.

### Pattern 3: cadence — speed-based, debounced moving/stationary classification

**What:** Use `gps.speed.kmph()` (RMC-derived, receiver's own velocity solution), not position deltas, with a hysteresis band (`STATIONARY_KMH` lower bound < `MOVING_KMH` upper bound) and a debounce window of N consecutive gate-accepted fixes before flipping state — directly implementing milestone `PITFALLS.md` Pitfall 7's recommendation.

**When to use:** Called only from `fix_gate`-accepted fixes (never on raw/ungated fixes).

**Example:**
```cpp
#ifndef GPS_MOVING_KMH
#define GPS_MOVING_KMH 5.0f        // above this: consider "moving" (safely above bench-observed 2.6 km/h jitter)
#endif
#ifndef GPS_STATIONARY_KMH
#define GPS_STATIONARY_KMH 3.0f    // below this: consider "stationary" (hysteresis gap avoids flapping)
#endif
#ifndef GPS_DEBOUNCE_FIXES
#define GPS_DEBOUNCE_FIXES 3       // consecutive gated fixes required before flipping state
#endif
#ifndef GPS_CADENCE_MOVING_S
#define GPS_CADENCE_MOVING_S 8     // emit interval while moving (spec range 5-10s; 8s = mid-range default)
#endif

void Cadence::onGatedFix(const Fix &f) {
    bool aboveMoving    = f.speedKmh > GPS_MOVING_KMH;
    bool belowStationary = f.speedKmh < GPS_STATIONARY_KMH;

    if (state_ == State::STATIONARY && aboveMoving) {
        if (++movingStreak_ >= GPS_DEBOUNCE_FIXES) { state_ = State::MOVING; movingStreak_ = 0; }
    } else if (state_ == State::MOVING && belowStationary) {
        if (++stationaryStreak_ >= GPS_DEBOUNCE_FIXES) { state_ = State::STATIONARY; stationaryStreak_ = 0; }
    } else {
        movingStreak_ = stationaryStreak_ = 0; // reset streaks on any non-confirming sample
    }

    if (state_ == State::MOVING) {
        if (millis() - lastEmittedMs_ >= (uint32_t)GPS_CADENCE_MOVING_S * 1000) {
            emit(f);
            lastEmittedMs_ = millis();
        }
    }
    // STATIONARY: no emission this phase (heartbeat cadence is an open product decision — see Open Questions)
}
```

### Anti-Patterns to Avoid

- **Checking only `gps.location.isValid()` without `age()`:** looks correct on the bench (fix acquired once, never lost) and silently accepts a fix that's minutes stale in the field — this is the exact latch behavior confirmed in Pattern 1.
- **Distance-delta movement detection:** flags stationary GPS jitter as movement (Pitfall 7); use `gps.speed.kmph()` instead.
- **Gating on `isUpdated()` instead of `isValid()`+`age()`:** `isUpdated()` only tells you a value was read since the last `updated=false` reset from a prior read — it's a "did I already consume this" flag, not a freshness/quality signal. Don't use it as a gate criterion.
- **Parsing GSA for 2D/3D `fixType` in this phase:** core TinyGPSPlus doesn't expose it; adding a `TinyGPSCustom` GSA parser is extra complexity GPS-02 doesn't require (GGA fix-quality + HDOP + sat count already covers the requirement).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|--------------|-----|
| NMEA sentence parsing / checksum verification | A custom `$GPRMC`/`$GNGGA` string splitter + checksum routine | TinyGPSPlus `encode()` char-streaming API (already integrated, hardware-validated) | TinyGPSPlus already handles multi-talker-ID matching (`GP`/`GN`/`GA`/`GB`/`GL` for RMC/GGA — confirmed by reading `TinyGPS++.cpp` line ~223), checksum validation, and partial/corrupted-sentence recovery (bench observed 10-17% checksum failures from a wiring issue and TinyGPSPlus already discards those cleanly via `passedChecksum()`/`failedChecksum()` counters, no crash/garbage data). |
| Great-circle distance for the outlier/implausible-speed check | Custom haversine formula | `TinyGPSPlus::distanceBetween(lat1, lon1, lat2, lon2)` static method | Already part of the library, avoids reimplementing/re-testing a well-known formula (a haversine implementation is also present as a reference pattern in `ESP32_IMU_BARO_GPS_VARIO-master/src/sensor/gps.cpp` if ever needed independently, but TinyGPSPlus's own method should be preferred since it's already in the dependency tree). |
| GPS module protocol auto-detection (UBX vs NMEA, baud probing) | A protocol-sniffing state machine (as seen in the `ESP32_IMU_BARO_GPS_VARIO` reference project, which targets UBX binary NAV-PVT at 115200) | Fixed NMEA @ 9600 baud (D-03) — no probing needed | D-02/D-03 already decided NMEA-only, no reconfiguration, single fixed baud — this class of complexity is explicitly out of scope for both target modules (ATGM336H has no UBX support at all; NEO-6M's UBX path isn't needed since NMEA already gives everything GPS-01/02/03 require). |

**Key insight:** Everything this phase needs is already exposed by TinyGPSPlus's public API over the already-working UART wiring — the actual engineering work is the *gating and cadence decision logic* around that data, not the parsing itself.

## Common Pitfalls

### Pitfall 1: `location.isValid()` never becomes `false` again after a fix is lost

**What goes wrong:** A `fix_gate` that checks only `isValid()` will accept fixes indefinitely after the receiver loses lock (antenna obstruction, tunnel, parking garage) — the position frozen at the last valid fix keeps passing the gate.

**Why it happens:** `TinyGPSLocation::commit()` sets `valid = updated = true` and nothing in the library resets `valid` to `false` on fix loss — confirmed by reading `TinyGPS++.cpp`'s `endOfTermHandler()`, which only ever calls `location.commit()` (never any reset) and only when `sentenceHasFix` is true for that sentence. `[VERIFIED: TinyGPS++.cpp source, vendored in this repo]`

**How to avoid:** Always pair `isValid()` with `age() < FRESH_MS` (Pattern 1). Pick `FRESH_MS` relative to the actual output rate (1 Hz per D-03) — e.g. 2.5x the expected interval gives headroom for one or two dropped/corrupted sentences (bench already shows a real-world 10-17% checksum failure rate from a wiring issue) without flagging every minor NMEA hiccup as "lost fix."

**Warning signs:** Serial output shows a fix that hasn't changed lat/lon for many seconds/minutes while the antenna is known to be obstructed.

### Pitfall 2: GGA fix-quality, satellite count, and HDOP commit independently of RMC/location

**What goes wrong:** `satellites.commit()` and `hdop.commit()` happen on *every* checksummed GGA sentence, regardless of `sentenceHasFix` — but `location`/`speed`/`course` only commit when the sentence actually reports a fix. A gate that reads `hdop`/`satellites` without also checking `location.isValid()`+`age()` could see "good HDOP, good sat count" values that are stale relative to the actual (lost) fix.

**Why it happens:** This is intentional in TinyGPSPlus (HDOP/sat count are diagnostic even without a fix), but it means the three groups of fields (`location`+`speed`+`course`, `date`+`time`, `satellites`+`hdop`) have *different* freshness lifecycles that must each be checked, not assumed to move together.

**How to avoid:** Gate on `location.isValid() && location.age() < FRESH_MS` as the primary freshness signal (it's the strictest — Pattern 2), and separately confirm `hdop.isValid()`/`satellites.isValid()` before reading their values (they could theoretically be `false` if no GGA sentence has ever been seen at all, e.g. a receiver misconfigured to only emit RMC).

**Warning signs:** HDOP/satellite count displayed values that don't change even though the fix has clearly gone stale.

### Pitfall 3: Speed/course only commit on RMC when `sentenceHasFix` — don't read them independent of the location freshness check

**What goes wrong:** Because `speed.commit()`/`course.commit()` are gated behind the same `sentenceHasFix` check as `location.commit()` (same `if` block in `endOfTermHandler`), they share `location`'s freshness lifecycle — but a caller who only checks `speed.isValid()` without checking `location.age()` could still read a stale speed value the same way as Pitfall 1.

**How to avoid:** Once `fix_gate` has confirmed a fix is fresh via `location`, it's safe to read `speed`/`course` from the same TinyGPSPlus instance in the same evaluation cycle — don't gate them separately with their own staleness logic (they update in lockstep with location by construction).

### Pitfall 4: ATGM336H's mixed-constellation talker IDs are already handled — but GSV's non-standard `BD` prefix is not, and doesn't need to be

**What goes wrong:** Worrying that `$BD*`-prefixed sentences (a non-standard BeiDou GSV variant some AT6558-based modules emit — talker byte `B`, not the `G`-prefixed `GB`/`GN`/`GP` used by standard NMEA) will break parsing or require special handling.

**Why it happens:** The context notes mention both `$GN*` and `$BD*` sentences from the ATGM336H. `[CITED: web search, LOW confidence — not verified against this exact module's actual serial capture]` GSV sentences (satellite-in-view lists) can appear with a non-standard `BD` prefix on some AT6558-based modules, while `RMC`/`GGA` (the only sentences this phase's logic needs) use the standard talker pattern (`GN` combined, or `GP` if configured GPS-only).

**How to avoid:** No action needed. TinyGPSPlus's talker-matching (`term[0]=='G' && strchr("PNABL", term[1])`, confirmed by reading the source) only applies to `RMC`/`GGA` — any sentence that doesn't match `GPS_SENTENCE_RMC`/`GPS_SENTENCE_GGA` (including a bare `$BD...` GSV variant, or standard `$..GSV`/`$..GSA`) is classified `GPS_SENTENCE_OTHER` and silently ignored by the core parser. This is not a bug to fix — it's exactly the intended behavior, and matches D-02's "NMEA-agnostic, don't need proprietary sentences" decision. **Recommend confirming on the actual bench capture** (log raw NMEA from both modules, `grep` for sentence prefixes) as a cheap verification step during implementation, since this claim is web-sourced rather than confirmed against this project's specific hardware.

### Pitfall 5: GPS speed noise floor while stationary requires hysteresis, not a single threshold

**What goes wrong:** A single "speed > threshold → moving" crossing point causes rapid state flapping right at the boundary, especially with cheap single-frequency receivers (NEO-6M, ATGM336H) that lack proprietary static-hold filtering (that requires UBX `CFG-NAV5`/`CFG-PM2`, out of scope per D-02).

**Why it happens:** Bench testing already observed ~2.6 km/h of speed jitter while stationary (per phase context). `[CITED: web search — u-blox stationary "position wander" is a documented characteristic of L1-only receivers without static-hold enabled; LOW-MEDIUM confidence, general/not device-specific]`

**How to avoid:** Use a hysteresis band (e.g. `MOVING_KMH=5.0` upper, `STATIONARY_KMH=3.0` lower — both safely clear of the observed 2.6 km/h jitter, with margin) plus a debounce count (N consecutive gated fixes, e.g. 3) before flipping state, per Pattern 3.

**Warning signs:** Serial log shows rapid MOVING/STATIONARY toggling while the unit is known to be sitting still.

## Code Examples

### TinyGPSPlus API reference for gating (verified against vendored source, `firmware/gps_tracker/.pio/libdeps/*/TinyGPSPlus/src/TinyGPS++.h`/`.cpp`)

| Accessor | Type/Range | Freshness lifecycle | Notes |
|----------|-----------|----------------------|-------|
| `gps.location.isValid()` | `bool` | **Never resets to `false`** once true | See Pattern 1 / Pitfall 1 — must pair with `age()` |
| `gps.location.age()` | `uint32_t` ms, or `ULONG_MAX` if never valid | Grows every loop until next commit | Primary staleness signal |
| `gps.location.FixQuality()` | `TinyGPSLocation::Quality` enum: `Invalid='0'`, `GPS='1'`, `DGPS='2'`, `PPS='3'`, `RTK='4'`, `FloatRTK='5'`, `Estimated='6'`, `Manual='7'`, `Simulated='8'` | Commits with `location` (RMC/GGA, only if `sentenceHasFix`) | From GGA field 6; note: calling this consumes the `updated` flag (side effect) |
| `gps.hdop.hdop()` | `double`, real decimal (e.g. `1.2`) — **already divided by 100** internally, do not re-scale | Commits on every checksummed GGA sentence, fix or no fix | From GGA field 8 |
| `gps.satellites.value()` | `uint32_t`, satellites *used* in the fix (not in view) | Commits on every checksummed GGA sentence, fix or no fix | From GGA field 7 |
| `gps.speed.kmph()` / `.mps()` | `double` | Commits with `location` (RMC only, `sentenceHasFix`) | Receiver's own Doppler-derived velocity — preferred movement signal over position-delta |
| `gps.course.deg()` | `double`, 0-360 | Commits with `location` (RMC only, `sentenceHasFix`) | |
| `gps.passedChecksum()` / `gps.failedChecksum()` | `uint32_t` counters | Cumulative since boot | Already used in the bench `main.cpp`; useful ongoing wiring-health indicator, not a per-fix gate input |
| `TinyGPSPlus::distanceBetween(lat1,lon1,lat2,lon2)` | static `double`, meters | — | Great-circle distance, for the outlier/implausible-speed check in Pattern 2 |

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|-------------------|---------------|--------|
| Position-delta movement detection | Speed-field (RMC/Doppler) movement detection | Long-standing GNSS engineering practice, not a recent change | Directly relevant here — avoids Pitfall 5/7 jitter-triggered false movement |
| UBX proprietary static-hold config | Plain NMEA + application-level hysteresis/debounce | This project's own D-02 decision (NMEA-agnostic requirement, both modules must work without proprietary config) | Confirms the hysteresis-band approach in Pattern 3 is the *required* approach here, not just a fallback |

**Deprecated/outdated:** N/A for this phase — no library or protocol deprecations apply to the Phase 1 scope.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|----------------|
| A1 | `GPS_HDOP_MAX = 5.0` as the reject threshold | Architecture Patterns §2 (Code Example) | Too loose: multipath-degraded fixes leak through to cadence/log; too strict: legitimate fixes rejected in marginal-sky-view mounts (parking garage, dock). Should be confirmed/tuned against a real field HDOP distribution during Phase 1 bench/vehicle testing, not shipped as final. |
| A2 | `GPS_SATS_MIN = 4` | Architecture Patterns §2 | 4 is the physical minimum for any fix at all; a stricter value (e.g. 5-6) may be warranted for logging-grade quality per general GNSS practice — needs field validation. |
| A3 | `GPS_MOVING_KMH = 5.0` / `GPS_STATIONARY_KMH = 3.0` hysteresis band | Architecture Patterns §3 | Bench-observed jitter (2.6 km/h) is a single-session, single-mount-location data point (near a window). Real vehicle/boat mount (worse sky view under a dash/hull) could show higher jitter, requiring a wider band. Must be validated per milestone `PITFALLS.md` Pitfall 7's explicit recommendation: multi-hour stationary test in the real mounting location. |
| A4 | `GPS_DEBOUNCE_FIXES = 3`, `GPS_WARMUP_FIXES = 3` | Architecture Patterns §2/§3 | Arbitrary starting points based on "a few seconds" guidance in `PITFALLS.md`; not empirically derived for this hardware. |
| A5 | `GPS_CADENCE_MOVING_S = 8` (default within the spec's 5-10s range) | Architecture Patterns §3 | GPS-03 only specifies a 5-10s range; picking a mid-range default is a product/UX choice deferred to Claude's Discretion per `01-CONTEXT.md` — could reasonably differ (e.g. 5s for finer trajectory fidelity vs. 10s for less flash write pressure once Phase 2 exists). |
| A6 | ATGM336H `$BD*` GSV-prefix claim, and `$GN`/`$GP` for RMC/GGA | Common Pitfalls §4 | Web-sourced (`[CITED: web search]`), not confirmed against this project's actual bench NMEA capture. If wrong in some other way (e.g. a genuinely non-`G`-prefixed RMC/GGA sentence), TinyGPSPlus would silently ignore it as `GPS_SENTENCE_OTHER` and no fix would ever be parsed from that module — low probability but should be a quick verification step (raw NMEA capture + grep) early in implementation. |
| A7 | Stationary heartbeat interval/behavior | Open Questions §1 | Not specified numerically by GPS-03 or `01-CONTEXT.md`; left as an explicit open product decision rather than assumed. |

## Open Questions

1. **Should "stationary" emit a low-rate heartbeat, or fully suppress emission?**
   - What we know: `01-CONTEXT.md`'s "Claude's Discretion" section explicitly lists "heartbeat parado" as a parameter to decide; GPS-03's success criteria only requires "fix acceptance pauses" when stationary, not a specific heartbeat behavior.
   - What's unclear: Whether Phase 1's bench-observable output needs *any* periodic stationary signal (e.g. every 60-120s, "still here, still parked") or whether pure suppression satisfies GPS-03 as written.
   - Recommendation: Default to pure suppression for Phase 1 (simplest, matches literal GPS-03 wording: "fix acceptance pauses"); expose a `GPS_STATIONARY_HEARTBEAT_S` constant (e.g. 120s, disabled by default / 0 = off) if the planner or a later phase (e.g. Phase 2's flash logger, which may want a periodic "still alive" record) needs it. Confirm with the user during planning if this is a hard requirement now vs. deferrable.

2. **Is GGA fix-quality `>= GPS (1)` (accepting plain standalone GPS, not requiring DGPS) the right threshold, or should it be stricter?**
   - What we know: Neither NEO-6M nor ATGM336H support SBAS/DGPS corrections in this project's configuration (no proprietary config per D-02) — they will essentially always report fix-quality `1` when they have any fix at all. Requiring `>= 2` (DGPS) would reject every fix from both modules.
   - What's unclear: Whether HDOP+satellite-count gating alone (without a stricter fix-quality floor) provides sufficient real-world reliability for a vehicle/boat trajectory log.
   - Recommendation: Accept fix-quality `>= 1` (GPS) as designed above — this is the only value these modules can realistically produce — and rely on HDOP (A1) + satellite count (A2) as the actual precision gates. Revisit only if field testing shows HDOP/sats alone are insufficient.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|-------------|-----------|---------|----------|
| PlatformIO CLI (`pio`) | Build/flash `firmware/gps_tracker/` | ✗ (not on `PATH` in this research session's shell) | — | Already confirmed working in the user's actual dev environment — the bench skeleton was hardware-validated 2026-08-01 (commit `540e11b`), so the toolchain exists there even though this sandboxed session can't see it. No firmware-level fallback needed; this is a research-environment limitation, not a project blocker. |
| TinyGPSPlus library | NMEA parsing (GPS-01/02) | ✓ | `1.1.0` (resolved, confirmed via vendored `library.json`) | — |
| ESP32 DevKit classic + ATGM336H/NEO-6M hardware | Bench validation of gate/cadence logic on real fixes | ✓ (per D-01/D-02/`01-CONTEXT.md` — user has both modules and will build per `gps_tracker_pinout.md`) | — | — |

**Missing dependencies with no fallback:** none — the one "missing" item (`pio` CLI in this research shell) is environmental to this research session, not a project dependency gap.

**Missing dependencies with fallback:** none needed.

## Security Domain

`security_enforcement: true`, `security_asvs_level: 1` per `.planning/config.json`. This phase is a self-contained embedded firmware component with no network exposure, no authentication surface, and no persistent storage — most ASVS categories don't apply yet (they become relevant in Phase 3+ once WiFi/MQTT/flash enter scope).

### Applicable ASVS Categories

| ASVS Category | Applies | Standard Control |
|----------------|---------|-------------------|
| V2 Authentication | No | No auth surface in this phase (single-device firmware, no network) |
| V3 Session Management | No | N/A |
| V4 Access Control | No | N/A |
| V5 Input Validation | Yes | NMEA sentences are untrusted external input (arrive over UART from a physical GPS module, which is itself parsing untrusted RF signals). TinyGPSPlus already performs checksum validation (`endOfTermHandler`'s parity check) before committing any field — `fix_gate` must never bypass this by reading raw serial bytes directly; always go through the library's validated, checksummed state. |
| V6 Cryptography | No | No crypto operations in this phase |

### Known Threat Patterns for this stack

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|-----------------------|
| Corrupted/garbled NMEA sentences (already observed on bench: 10-17% checksum failure from a loose TX wire) | Tampering (unintentional, but same defensive posture applies) | TinyGPSPlus's per-sentence checksum verification (already in place) — `fix_gate` must never trust a field whose owning sentence failed checksum; this is automatic since failed-checksum sentences never call `commit()`, so stale-but-valid-looking data (not corrupted garbage) is the actual risk surface, covered by Pattern 1/2's freshness+quality gating. |
| Spoofed/implausible GPS fix (multipath jump, "null island" `(0,0)`, or genuinely malicious GPS spoofing in a vehicle-tracking threat model) | Tampering / Spoofing | The outlier/implausible-speed check in Pattern 2 (`GPS_MAX_PLAUSIBLE_KMH`) catches large single-sample jumps; explicit `(0,0)` rejection is implicitly covered since a `(0,0)` jump from any real position would almost always exceed the plausible-speed threshold, but consider an explicit `lat==0 && lon==0` reject as a cheap, unambiguous extra guard per milestone `PITFALLS.md` Pitfall 4's "null island" mention. |

## Sources

### Primary (HIGH confidence)
- `firmware/gps_tracker/.pio/libdeps/esp32-c3-supermini/TinyGPSPlus/src/TinyGPS++.h` and `.cpp` (read directly, this repo's vendored build artifact) — `isValid()`/`age()`/`isUpdated()` semantics, `commit()` gating logic, talker-ID matching (`GP`/`GN`/`GA`/`GB`/`GL`), HDOP scaling (`value()/100.0`), GGA/RMC field-to-accessor mapping
- `firmware/gps_tracker/.pio/libdeps/esp32-c3-supermini/TinyGPSPlus/library.json` / `library.properties` (read directly) — resolved version `1.1.0`, author/repo confirmation
- `firmware/gps_tracker/src/main.cpp` (read directly, hardware-validated bench skeleton) — existing UART wiring pattern, `passedChecksum()`/`failedChecksum()` usage, LED status pattern
- `firmware/gps_tracker/gps_tracker_pinout.md`, `firmware/gps_tracker/platformio.ini` (read directly) — pin assignments and existing `lib_deps`/`build_flags` conventions for both target envs
- `.planning/phases/01-gps-acquisition-adaptive-cadence/01-CONTEXT.md`, `.planning/REQUIREMENTS.md`, `.planning/STATE.md` (read directly) — locked decisions, requirement IDs, project history
- `.planning/research/PITFALLS.md`, `ARCHITECTURE.md`, `STACK.md` (read directly, milestone-level) — Pitfall 4/7 fix-quality-before-cadence rationale, recommended module structure, library recommendations
- `/home/luc/Dev/ESP32_IMU_BARO_GPS_VARIO-master/src/sensor/gps.h`/`.cpp` (read directly, reference project) — cross-reference only; this project uses UBX binary NAV-PVT (not applicable to this phase's NMEA-agnostic decision) but confirms standard `fixType` semantics (`FIX_NONE`/`FIX_2D`/`FIX_3D`) and provides a haversine-distance reference pattern

### Secondary (MEDIUM confidence)
- *(none this phase — the numeric-threshold web searches below did not cross-check against a second independent authoritative source within this session, so they're classified LOW/tertiary rather than MEDIUM)*

### Tertiary (LOW confidence)
- Web search: "HDOP satellite count threshold GPS fix quality filtering vehicle tracker NMEA" — GGA fix-quality codes (`0`-`6`), general HDOP-mask practice (`≤2` survey-grade, `~6` common logging cutoff), 4-satellite minimum / 6+ recommended. Sources surfaced: docs.fixposition.com, terraflow.ca, marinepublic.com, m2msupport.net.
- Web search: "ATGM336H AT6558 NMEA output talker ID GN GB GSA GSV sentences" — confirms `GN`/`GP` talker for RMC/GGA depending on constellation config, and a non-standard `BD`-prefixed GSV variant. Sources: electrodragon.com, wiki.millerjs.org, forum.arduino.cc (`Configuration of chinese ATGM336 GNSS`).
- Web search: "GPS speed jitter stationary u-blox NEO-6M km/h accuracy noise" — "position wander" terminology, UBX static-hold as the (out-of-scope) proprietary mitigation, typical 2-5m open-sky accuracy. Sources: zbotic.in, u-blox portal Q&A, content.u-blox.com receiver description.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — TinyGPSPlus is already the working, hardware-validated dependency; nothing new introduced
- Architecture (module structure, gate/cadence pattern): HIGH — directly derived from reading the actual library source plus the milestone-level `ARCHITECTURE.md`/`PITFALLS.md` already researched for this exact project
- Numeric thresholds (HDOP/sats/speed/debounce values): LOW-MEDIUM — reasonable starting points grounded in general GNSS practice and the single bench data point (2.6 km/h jitter), explicitly flagged in Assumptions Log for field-test confirmation before being treated as final

**Research date:** 2026-08-01
**Valid until:** Stable — TinyGPSPlus API and NMEA semantics don't churn; numeric thresholds should be revisited after Phase 1's own field/bench verification (not time-based staleness, but evidence-based)
