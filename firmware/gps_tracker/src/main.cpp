// GPS Tracker — bench harness (Fase 1)
// Wires GpsReader -> FixGate -> Cadence -> serial output. Every snapshot
// returned by poll() goes through FixGate::evaluate() before anything else
// happens with it; only the GateResult::ACCEPT branch may forward a fix
// downstream, and cadence's single call site lives inside that branch
// (Phase 2's flash logger attaches after cadence next). LED onboard: solid
// = most recent evaluation was ACCEPT, blinking = bytes arriving but the
// last result was a rejection, dark = no data at all.

#include <Arduino.h>
#include <math.h>
#include "gps_config.h"
#include "gps_types.h"
#include "gps_reader.h"
#include "fix_gate.h"
#include "cadence.h"
#include "i2c_bus.h"
#include "accel_sensor.h"

#ifndef LED_ACTIVE_LOW
#define LED_ACTIVE_LOW 0
#endif

static const uint8_t kGateResultCount = static_cast<uint8_t>(GateResult::REJECT_OUTLIER_JUMP) + 1;

static GpsReader  gpsReader;
static FixGate    fixGate;
static Cadence    cadence;

static uint32_t   emitCount    = 0;
static uint32_t   suppressCount = 0;

static uint32_t   lastHealthMs = 0;
static uint32_t   lastAccelPollMs = 0;
static uint32_t   accelWakeCount = 0;   // accumulated accel_sensor::wakeEdge() edges since boot, for [HEALTH]
static uint32_t   gateCounters[kGateResultCount] = {0};

// Reject-line dedup state is kept per-origin (real vs. [SIM]) so a repeated
// reason on one source can never silently suppress the first occurrence of
// that same reason from the other source (WR-04) -- index 0 = real fixes,
// index 1 = "[SIM] "-origin fixes.
static GateResult lastPrintedReason[2] = { GateResult::ACCEPT, GateResult::ACCEPT };
static bool       havePrintedReason[2] = { false, false };
static GateResult lastResult        = GateResult::REJECT_NO_FIX;   // no data yet

static Fix       lastFix{};
static bool      everAccepted    = false;
static uint32_t  lastAcceptedAtMs = 0;

// Tracks the last *gate-accepted* fix only (set exclusively inside
// handleAccept()), as opposed to lastFix above which is the last raw poll()
// snapshot regardless of gate outcome. The simulator's first-activation
// anchor must seed from this, not from lastFix, so a rejected raw sample
// (e.g. REJECT_NULL_ISLAND, REJECT_HDOP) can never poison its start position.
static Fix       lastAcceptedFix{};
static bool      haveAcceptedFix  = false;

static inline void ledWrite(bool on) {
    digitalWrite(LED_PIN, LED_ACTIVE_LOW ? !on : on);
}

// Every print helper takes a leading `origin` tag ("" for real fixes,
// "[SIM] " for the bench simulator below) so a captured log can never
// confuse a synthetic line for a real one.

// 0 = real fixes, 1 = "[SIM] "-origin fixes -- see the dedup-state comment
// above lastPrintedReason/havePrintedReason.
static inline uint8_t originIndex(const char *origin) {
    return (origin[0] == '\0') ? 0 : 1;
}

// Returns true only when this call actually printed a new line (i.e. the
// reason changed since the last call for this origin) -- callers that drive
// a per-loop-iteration watchdog (rather than a once-per-actual-Fix reject
// path) use this as the edge signal for their own counters (WR-01), since
// loop() can re-run this thousands of times per second for a single
// lost/never-acquired fix.
static bool printRejectIfNew(const char *origin, GateResult result, float hdop, uint8_t sats, uint32_t ageMs, uint8_t quality) {
    uint8_t idx = originIndex(origin);
    if (havePrintedReason[idx] && result == lastPrintedReason[idx]) return false;   // dedup consecutive same-reason, per origin
    Serial.printf("%s[REJECT] reason=%s hdop=%.1f sats=%u age=%lums q=%u\n",
                  origin, gateResultName(result), hdop, sats, (unsigned long)ageMs, quality);
    lastPrintedReason[idx] = result;
    havePrintedReason[idx] = true;
    return true;
}

static void printEmit(const char *origin, CadenceAction action, const Fix &f, CadenceState state) {
    const char *tag = (action == CadenceAction::EMIT_HEARTBEAT) ? "[EMIT] hb=1" : "[EMIT]";
    Serial.printf(
        "%s%s t=%lu lat=%.6f lon=%.6f alt=%.1fm spd=%.1fkm/h crs=%.0f hdop=%.1f sats=%u q=%u age=%lums state=%s\n",
        origin, tag, (unsigned long)f.utc_unix, f.lat, f.lon, f.alt_m,
        f.speed_kmh, f.course_deg, f.hdop, f.sats, f.fix_quality,
        (unsigned long)f.age_ms, cadenceStateName(state));
}

static void printSuppress(const char *origin, CadenceAction action, const Fix &f, CadenceState state) {
    Serial.printf("%s[SUPPRESS] reason=%s spd=%.1fkm/h state=%s\n",
                  origin, cadenceActionName(action), f.speed_kmh, cadenceStateName(state));
}

static void printStateChange(const char *origin, CadenceState oldState, CadenceState newState, float speedKmh, uint32_t elapsedS) {
    Serial.printf("%s[STATE] %s -> %s spd=%.1fkm/h elapsed_in_prev_state=%lus\n",
                  origin, cadenceStateName(oldState), cadenceStateName(newState),
                  speedKmh, (unsigned long)elapsedS);
}

// handleAccept() is the single call site for cadence's state machine.
// Called only from inside a `result == GateResult::ACCEPT` branch — the
// real-fix path in loop() and the bench simulator's simTick() below both
// gate on GateResult::ACCEPT before reaching here, so an ungated (or
// gate-rejected) fix can never flip cadence's moving/stationary state.
static void handleAccept(const Fix &f, const char *origin) {
    everAccepted      = true;
    lastAcceptedAtMs  = f.mono_ms;
    lastAcceptedFix   = f;
    haveAcceptedFix   = true;
    havePrintedReason[originIndex(origin)] = false;   // any later rejection on this origin always prints its first line

    CadenceState  prevCadenceState = cadence.state();
    CadenceAction cadenceAction    = cadence.onGatedFix(f);

    if (cadence.stateChanged()) {
        printStateChange(origin, prevCadenceState, cadence.state(),
                          cadence.lastTransitionSpeedKmh(), cadence.lastTransitionElapsedS());
    }

    if (cadenceAction == CadenceAction::EMIT || cadenceAction == CadenceAction::EMIT_HEARTBEAT) {
        emitCount++;
        printEmit(origin, cadenceAction, f, cadence.state());
    } else {
        suppressCount++;
        printSuppress(origin, cadenceAction, f, cadence.state());
    }
}

// ---------------------------------------------------------------------------
// Bench simulator (GPS-02/GPS-03 provable at a desk). Compiled only when
// GPS_BENCH_SIM is non-zero; see the header comment in gps_config.h — this
// flag must be 0 before any networked build (Phase 3 onward). The simulator
// synthesises one Fix per second and routes it through the identical
// FixGate::evaluate() -> handleAccept() (cadence) pipeline the real fixes
// take, never a shortcut around either. Every line it produces carries the
// [SIM] marker so a captured log can never mistake synthetic data for a
// real fix.
#if GPS_BENCH_SIM

enum class SimInject : uint8_t { NONE, JUMP, NULL_ISLAND, BAD_QUALITY };

static const uint32_t kSimBaseEpochS = 1700000000UL;   // arbitrary fixed epoch, [SIM] t= field only
static const double   kSimDefaultLat = -22.919952;      // known bench coordinates (01-RESEARCH.md)
static const double   kSimDefaultLon = -43.215005;

static bool       simActive_        = false;
static float      simSpeedKmh_      = 0.0f;
static double     simLat_           = 0.0;
static double     simLon_           = 0.0;
static bool       simHavePosition_  = false;
static uint32_t   simLastTickMs_    = 0;
static uint32_t   simStartMs_       = 0;
static SimInject  simPendingInject_ = SimInject::NONE;

static inline bool simIsActive() { return simActive_; }

// Metres per degree of longitude at a given latitude (equirectangular
// approximation) -- good enough at bench/vehicle scale near the equator,
// used only to advance the simulator's due-east trajectory.
static double simMetersPerDegreeLon(double latDeg) {
    return 111320.0 * cos(latDeg * DEG_TO_RAD);
}

static void simPrintHelp() {
    Serial.println("[SIM] [HELP] r  toggle raw NMEA echo");
    Serial.println("[SIM] [HELP] h  show this help");
    Serial.println("[SIM] [HELP] s  toggle bench simulator on/off (1 fix/s while on)");
    Serial.println("[SIM] [HELP] +  raise simulated speed by 2 km/h (clamped 0-150)");
    Serial.println("[SIM] [HELP] -  lower simulated speed by 2 km/h (clamped 0-150)");
    Serial.println("[SIM] [HELP] x  inject one outlier-jump fix -> REJECT_OUTLIER_JUMP");
    Serial.println("[SIM] [HELP] z  inject one (0,0) fix -> REJECT_NULL_ISLAND");
    Serial.println("[SIM] [HELP] b  inject one bad-quality fix (HDOP+sats) -> REJECT_HDOP");
}

static void simAdjustSpeed(float deltaKmh) {
    simSpeedKmh_ += deltaKmh;
    if (simSpeedKmh_ < 0.0f)   simSpeedKmh_ = 0.0f;
    if (simSpeedKmh_ > 150.0f) simSpeedKmh_ = 150.0f;
    Serial.printf("[SIM] speed=%.1fkm/h\n", simSpeedKmh_);
}

static void simToggle() {
    simActive_ = !simActive_;
    // The real path and the simulator share one FixGate instance; its
    // last-accepted-position reference must never survive a source switch
    // unreconciled, or the first fix from the newly-active source gets
    // measured against the other source's (possibly far-away) last
    // position and permanently locked out by REJECT_OUTLIER_JUMP (CR-02).
    fixGate.reset();
    if (simActive_) {
        if (!simHavePosition_) {
            // Anchor at the last real *accepted* fix when one exists,
            // otherwise the known bench coordinates. Using lastFix (the
            // last raw poll() snapshot) here would risk seeding from a fix
            // that itself failed the gate (e.g. null-island, bad HDOP),
            // which can trigger a spurious REJECT_OUTLIER_JUMP on the
            // simulator's very first tick (WR-02).
            if (haveAcceptedFix) {
                simLat_ = lastAcceptedFix.lat;
                simLon_ = lastAcceptedFix.lon;
            } else {
                simLat_ = kSimDefaultLat;
                simLon_ = kSimDefaultLon;
            }
            simHavePosition_ = true;
        }
        simStartMs_    = millis();
        simLastTickMs_ = simStartMs_;
        Serial.printf("[SIM] simulator ON speed=%.1fkm/h lat=%.6f lon=%.6f\n",
                      simSpeedKmh_, simLat_, simLon_);
    } else {
        Serial.println("[SIM] simulator OFF -- resuming real fixes");
    }
}

static void simHandleCommand(char c) {
    switch (c) {
        case 'h': simPrintHelp(); break;
        case 's': simToggle(); break;
        case '+': simAdjustSpeed(2.0f); break;
        case '-': simAdjustSpeed(-2.0f); break;
        case 'x':
            simPendingInject_ = SimInject::JUMP;
            Serial.println("[SIM] next fix: outlier jump");
            break;
        case 'z':
            simPendingInject_ = SimInject::NULL_ISLAND;
            Serial.println("[SIM] next fix: null island (0,0)");
            break;
        case 'b':
            simPendingInject_ = SimInject::BAD_QUALITY;
            Serial.println("[SIM] next fix: bad quality (HDOP+sats)");
            break;
        default: break;   // unknown/whitespace chars ignored
    }
}

// Advances the simulator by exactly one synthetic Fix when a full second
// has elapsed, and routes it through FixGate::evaluate() then
// handleAccept() -- the same pipeline poll()-sourced real fixes take.
static void simTick() {
    if (!simActive_) return;

    uint32_t now = millis();
    if (now - simLastTickMs_ < 1000) return;
    uint32_t dtMs = now - simLastTickMs_;
    simLastTickMs_ = now;

    SimInject inject   = simPendingInject_;
    simPendingInject_  = SimInject::NONE;

    Fix f{};
    f.course_deg  = 90.0f;
    f.alt_m       = 10.0f;
    f.fix_quality = 1;
    f.age_ms      = 0;
    f.mono_ms     = now;
    f.utc_unix    = kSimBaseEpochS + (now - simStartMs_) / 1000;
    f.speed_kmh   = simSpeedKmh_;
    f.hdop        = 1.0f;
    f.sats        = 8;

    switch (inject) {
        case SimInject::JUMP:
            // Displace ~1 degree of latitude (~111 km) in a single 1 Hz
            // tick -- far beyond DEFAULT_GPS_MAX_PLAUSIBLE_KMH for any dt
            // this loop produces. The continuity anchor (simLat_/simLon_)
            // is left untouched, exactly mirroring fix_gate's own behaviour
            // of never poisoning its last-accepted reference on reject, so
            // the next normal tick resumes from where the simulator really
            // is.
            f.lat = simLat_ + 1.0;
            f.lon = simLon_;
            break;
        case SimInject::NULL_ISLAND:
            f.lat = 0.0;
            f.lon = 0.0;
            break;
        case SimInject::BAD_QUALITY:
        case SimInject::NONE:
        default: {
            // Advance the continuity anchor consistently with the reported
            // speed along a due-east bearing -- holding position while
            // claiming nonzero speed, or teleporting, would each falsify
            // the plausibility check this simulator exists to exercise.
            double distM = (simSpeedKmh_ * 1000.0 / 3600.0) * (dtMs / 1000.0);
            double dLon  = distM / simMetersPerDegreeLon(simLat_);
            simLon_ += dLon;
            f.lat = simLat_;
            f.lon = simLon_;
            if (inject == SimInject::BAD_QUALITY) {
                f.hdop = DEFAULT_GPS_HDOP_MAX + 1.0f;
                f.sats = (DEFAULT_GPS_SATS_MIN > 0) ? (DEFAULT_GPS_SATS_MIN - 1) : 0;
            }
            break;
        }
    }

    GateResult result = fixGate.evaluate(f);
    gateCounters[static_cast<uint8_t>(result)]++;
    lastResult = result;
    lastFix    = f;

    if (result == GateResult::ACCEPT) {
        handleAccept(f, "[SIM] ");
    } else {
        printRejectIfNew("[SIM] ", result, f.hdop, f.sats, f.age_ms, f.fix_quality);
    }
}

#else   // !GPS_BENCH_SIM -- stubs so loop()/setup() compile unchanged

static inline bool simIsActive() { return false; }
static inline void simTick() {}
static inline void simHandleCommand(char) {}
static inline void simPrintHelp() {}

#endif  // GPS_BENCH_SIM

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    gpsReader.begin();
    Serial.println();
    Serial.printf("[gps_tracker] bench harness -- UART%d RX=%d TX=%d @ %d\n",
                  GPS_UART_NUM, GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    Serial.println("[gps_tracker] 'r' = toggle raw NMEA echo");
#if GPS_BENCH_SIM
    Serial.println("[gps_tracker] 'h' = bench simulator help (GPS_BENCH_SIM=1)");
#endif

    // The I2C bus init below runs AFTER gpsReader.begin() and the banner
    // lines above: the GPS UART must already be open and draining before
    // any I2C transaction competes for CPU, so the boot scan is never the
    // first thing racing the GPS pipeline (Fase 01.1, D-12).
    Serial.printf("[gps_tracker] I2C bus on sda=%d scl=%d\n",
                  static_cast<int>(DEFAULT_I2C_SDA_PIN), static_cast<int>(DEFAULT_I2C_SCL_PIN));
    i2c_bus::begin();

    bool accelOk = accel_sensor::begin();
    Serial.printf("[gps_tracker] MPU6050 accel: %s\n", accelOk ? "OK" : "not found");
}

// Drives i2c_bus's runtime recovery: tick() advances the round-robin retry
// scan (at most one address probe per call, see i2c_bus.cpp), and any
// module i2c_bus flags as reinitDue() gets its driver re-attached here. This
// plan (02) wires ACCEL's begin() call into its case below; planos 03 (MAG,
// ENV) and 04 (DISPLAY) still each insert their own begin() call before
// setOnline() in the matching case, so this block stays additive rather
// than needing a rewrite each time a driver lands.
static void i2cRecoverTick() {
    i2c_bus::tick();

    for (uint8_t i = 0; i < static_cast<uint8_t>(I2cModule::COUNT); i++) {
        I2cModule m = static_cast<I2cModule>(i);
        if (!i2c_bus::reinitDue(m)) continue;

        switch (m) {
            case I2cModule::DISPLAY:
                // Plano 04 insere display::begin(i2c_bus::boundAddr(m)) aqui antes do setOnline.
                i2c_bus::setOnline(m, true);
                break;
            case I2cModule::ACCEL:
                // Re-runs the WHO_AM_I + sleep-exit sequence so D-13 recovery
                // actually re-initialises the chip, not just flips the flag.
                accel_sensor::begin();
                i2c_bus::setOnline(m, true);
                break;
            case I2cModule::MAG:
                // Plano 03 insere mag_sensor::begin(i2c_bus::boundAddr(m)) aqui antes do setOnline.
                i2c_bus::setOnline(m, true);
                break;
            case I2cModule::ENV:
                // Plano 03 insere env_sensor::begin(i2c_bus::boundAddr(m)) aqui antes do setOnline.
                i2c_bus::setOnline(m, true);
                break;
            default:
                break;
        }

        Serial.printf("[I2C] %s recovered addr=0x%02X\n", i2c_bus::moduleName(m), i2c_bus::boundAddr(m));
        i2c_bus::clearReinit(m);
    }
}

void loop() {
    Fix  fix;
    bool haveFix = gpsReader.poll(fix);

    if (Serial.available()) {
        char c = (char)Serial.read();
        if (c == 'r') {
            gpsReader.setRawEcho(!gpsReader.rawEcho());
            Serial.printf("\n[gps_tracker] raw echo: %s\n", gpsReader.rawEcho() ? "ON" : "OFF");
        } else {
            simHandleCommand(c);
        }
    }

    // While the simulator is on, real fixes are ignored for gate/cadence
    // purposes so the two sources cannot interleave into a nonsensical
    // trajectory; gpsReader still counts their bytes/checksums into
    // [HEALTH] regardless, since poll() above always runs.
    if (!simIsActive()) {
        if (haveFix) {
            lastFix = fix;

            GateResult result = fixGate.evaluate(fix);
            gateCounters[static_cast<uint8_t>(result)]++;
            lastResult = result;

            if (result == GateResult::ACCEPT) {
                handleAccept(fix, "");
            } else {
                printRejectIfNew("", result, fix.hdop, fix.sats, fix.age_ms, fix.fix_quality);
            }
        } else {
            // No new snapshot this tick. TinyGPSPlus's location validity/updated
            // flags are a one-way latch (01-RESEARCH.md Pattern 1) — once a fix
            // is fully lost mid-session, location never commits again, so
            // poll() alone can never surface that transition. This watchdog
            // reuses fix_gate's own DEFAULT_GPS_FRESH_MS window to detect a
            // frozen fix in real time (mitigates threat T-01-02), and covers
            // the symmetric cold-boot case where no fix has ever landed.
            //
            // This branch re-runs on every loop() iteration where poll()
            // returned false -- thousands of times per second, not once per
            // lost/never-acquired fix -- so the gateCounters increment below
            // is gated on printRejectIfNew()'s edge signal (only true when
            // the reason actually changed) rather than unconditional, or
            // no_fix=/stale= would dwarf every other [HEALTH] counter and
            // could wrap a uint32_t within the field lifetime of the device
            // (WR-01).
            if (!everAccepted) {
                if (printRejectIfNew("", GateResult::REJECT_NO_FIX, lastFix.hdop, lastFix.sats, 0, lastFix.fix_quality)) {
                    gateCounters[static_cast<uint8_t>(GateResult::REJECT_NO_FIX)]++;
                }
                lastResult = GateResult::REJECT_NO_FIX;
            } else if ((millis() - lastAcceptedAtMs) >= DEFAULT_GPS_FRESH_MS) {
                uint32_t ageMs = millis() - lastAcceptedAtMs;
                if (printRejectIfNew("", GateResult::REJECT_STALE, lastFix.hdop, lastFix.sats, ageMs, lastFix.fix_quality)) {
                    gateCounters[static_cast<uint8_t>(GateResult::REJECT_STALE)]++;
                }
                lastResult = GateResult::REJECT_STALE;
            }
        }
    }

    simTick();

    // Accel wake poll (D-05/D-06). This block is the ONLY place in the
    // codebase that invokes Cadence's accelerometer wake entry point --
    // mirrors handleAccept() being the sole call site of the GPS-gated one.
    // Runs after the GPS/simTick pass above and before i2cRecoverTick()
    // below, so the GPS pipeline always gets first claim on this iteration.
    if (millis() - lastAccelPollMs >= DEFAULT_ACCEL_POLL_MS) {
        lastAccelPollMs = millis();
        accel_sensor::poll();

        if (accel_sensor::wakeEdge()) {
            CadenceState prevCadenceState = cadence.state();
            if (cadence.onAccelWake(millis())) {
                accelWakeCount++;
                Serial.printf("[ACCEL] wake mag=%.2fg streak=%u\n",
                              accel_sensor::magnitudeG(), accel_sensor::debounceStreak());
                printStateChange("", prevCadenceState, cadence.state(),
                                  cadence.lastTransitionSpeedKmh(), cadence.lastTransitionElapsedS());
            }
        }
    }

    // Peripheral work always runs after the GPS pass above (poll/gate/
    // cadence/simTick already processed this iteration's fix snapshot) and
    // before the LED/[HEALTH] block -- the GPS pipeline never waits on I2C.
    i2cRecoverTick();

    if (lastResult == GateResult::ACCEPT) {
        ledWrite(true);
    } else if (gpsReader.receiving()) {
        ledWrite((millis() / 250) % 2);
    } else {
        ledWrite(false);
    }

    if (millis() - lastHealthMs >= DEFAULT_GPS_HEALTH_PERIOD_MS) {
        lastHealthMs = millis();
        uint32_t i2cDrops = i2c_bus::offlineEvents(I2cModule::DISPLAY) +
                            i2c_bus::offlineEvents(I2cModule::ACCEL) +
                            i2c_bus::offlineEvents(I2cModule::MAG) +
                            i2c_bus::offlineEvents(I2cModule::ENV);

        Serial.printf(
            "[HEALTH] bytes=%lu ok=%lu bad=%lu sats=%u hdop=%.1f rx=%s"
            " accept=%lu no_fix=%lu stale=%lu null_island=%lu time=%lu hdop_rej=%lu sats_rej=%lu warmup=%lu jump=%lu"
            " cadence=%s since_emit=%lus emit=%lu suppress=%lu"
            " disp=%s accel=%s mag=%s env=%s i2c_drops=%lu"
            " accel_g=%.2f accel_wakes=%lu\n",
            (unsigned long)gpsReader.bytesRead(),
            (unsigned long)gpsReader.passedChecksum(),
            (unsigned long)gpsReader.failedChecksum(),
            lastFix.sats, lastFix.hdop,
            gpsReader.receiving() ? "yes" : "no",
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::ACCEPT)],
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::REJECT_NO_FIX)],
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::REJECT_STALE)],
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::REJECT_NULL_ISLAND)],
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::REJECT_TIME)],
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::REJECT_HDOP)],
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::REJECT_SATS)],
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::REJECT_WARMUP)],
            (unsigned long)gateCounters[static_cast<uint8_t>(GateResult::REJECT_OUTLIER_JUMP)],
            cadenceStateName(cadence.state()),
            (unsigned long)cadence.secondsSinceEmit(),
            (unsigned long)emitCount,
            (unsigned long)suppressCount,
            i2c_bus::online(I2cModule::DISPLAY) ? "on" : "off",
            i2c_bus::online(I2cModule::ACCEL) ? "on" : "off",
            i2c_bus::online(I2cModule::MAG) ? "on" : "off",
            i2c_bus::online(I2cModule::ENV) ? "on" : "off",
            (unsigned long)i2cDrops,
            accel_sensor::magnitudeG(),
            (unsigned long)accelWakeCount);
    }
}
