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
#include <string.h>
#include "gps_config.h"
#include "gps_types.h"
#include "gps_reader.h"
#include "fix_gate.h"
#include "cadence.h"
#include "i2c_bus.h"
#include "accel_sensor.h"
#include "mag_sensor.h"
#include "env_sensor.h"
#include "display.h"
#include "net_config.h"
#include "net_state.h"
#include "net_store.h"
#include "portal.h"
#include "telemetry.h"

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
static uint32_t   lastMagPollMs = 0;
static uint32_t   lastEnvPollMs = 0;
static uint32_t   lastDisplayMs = 0;
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
    Serial.println("[SIM] [HELP] cal + Enter  start hard-iron calibration capture; rotate the "
                    "compass module slowly through one full horizontal turn; send 'cal' + Enter "
                    "again to save offsets to NVS (D-09)");
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

    bool magOk = mag_sensor::begin();
    Serial.printf("[gps_tracker] compass: %s chip=%s\n", magOk ? "OK" : "not found", mag_sensor::chipName());

    bool envOk = env_sensor::begin();
    Serial.printf("[gps_tracker] HTU21D env: %s\n", envOk ? "OK" : "not found");

    bool dispOk = display::begin();
    Serial.printf("[gps_tracker] OLED display: %s addr=0x%02X\n", dispOk ? "OK" : "not found", display::addr());

    // Network comes up last: the AP + DNS + HTTP server and the WiFi stack
    // all allocate, and doing that after the I2C probes above keeps the boot
    // ordering of Fase 01.1 (GPS UART draining first, then peripherals)
    // untouched. portal::begin() is what sets WIFI_AP_STA -- telemetry never
    // changes the mode, so the field AP survives every station transition.
    bool portalOk = portal::begin();
    Serial.printf("[gps_tracker] portal captive: %s ssid=\"%s\" http://192.168.4.1/\n",
                  portalOk ? "OK" : "FALHOU", portal::apSsid());

    telemetry::begin(portal::creds());
    if (net_store::haveWifi(portal::creds())) {
        // Comprimentos, nunca o conteúdo: é o suficiente para apanhar um
        // campo truncado ou com espaço a mais vindo do formulário, sem
        // imprimir a senha na serial. Se o número não bater com o que foi
        // digitado, o bug é no POST/NVS, não na credencial.
        Serial.printf("[gps_tracker] wifi de casa: \"%s\" (ssid %u chars) senha=%s (%u chars)"
                      " broker=%s:%u user=%s\n",
                      portal::creds().wifi_ssid, (unsigned)strlen(portal::creds().wifi_ssid),
                      portal::creds().wifi_pass[0] ? "definida" : "VAZIA",
                      (unsigned)strlen(portal::creds().wifi_pass),
                      portal::creds().mqtt_host, (unsigned)portal::creds().mqtt_port,
                      portal::creds().mqtt_user[0] ? portal::creds().mqtt_user : "(anon)");
    } else {
        Serial.println("[gps_tracker] sem wifi configurado -- configurar em http://192.168.4.1/");
    }
}

// Speed-unit conversion (D-04). Both functions branch only on the
// DEFAULT_SPEED_UNIT build-time constant -- never on a runtime variable --
// so the compiler folds the branch away and the unit is fixed for the whole
// binary, selectable only by a platformio.ini build_flags override.
static float speedInDisplayUnit(float kmh) {
    if (DEFAULT_SPEED_UNIT == 0) return kmh;
    return kmh * 0.539957f;   // km/h -> knots
}

static const char *speedUnitLabel() {
    if (DEFAULT_SPEED_UNIT == 0) return "km/h";
    return "kt";
}

// Assembles one complete display frame from state main.cpp already tracks
// plus each peripheral's own accessor -- display.cpp itself never touches
// any of these modules directly (see display.h). Called only from the
// render block in loop(), after the GPS/simTick pass and every peripheral
// poll for this iteration have already run, so the frame it builds always
// reflects this iteration's fix -- never a half-processed one (D-02).
static DisplayState buildDisplayState() {
    DisplayState s{};

    // Same freshness window as the REJECT_STALE watchdog above: losing the
    // fix mid-session must fall back to the D-03 acquisition screen instead
    // of freezing the last known values on screen.
    bool haveFreshFix = everAccepted && (millis() - lastAcceptedAtMs) < DEFAULT_GPS_FRESH_MS;

    s.have_fix      = haveFreshFix;
    s.sats_used     = lastFix.sats;
    s.sats_in_view  = gpsReader.satsInView();
    s.hdop          = lastFix.hdop;
    s.receiving_nmea = gpsReader.receiving();

    s.speed_display = speedInDisplayUnit(lastFix.speed_kmh);
    s.speed_unit    = speedUnitLabel();

    bool fromCompass = false;
    s.heading_deg = mag_sensor::arbitratedHeading(lastFix.speed_kmh, lastFix.course_deg,
                                                   haveFreshFix, &fromCompass);
    s.heading_from_compass = fromCompass;
    s.heading_cardinal     = mag_sensor::cardinal(s.heading_deg);

    EnvReading env = env_sensor::last();
    s.temp_c    = env.temp_c;
    s.hum_pct   = env.hum_pct;
    s.env_valid = env.valid;

    s.accel_g = accel_sensor::magnitudeG();

    s.cadence_moving = (cadence.state() == CadenceState::MOVING);

    s.uptime_s = millis() / 1000;

    s.accel_online = i2c_bus::online(I2cModule::ACCEL);
    s.mag_online   = i2c_bus::online(I2cModule::MAG);
    s.env_online   = i2c_bus::online(I2cModule::ENV);

    return s;
}

// Network sibling of buildDisplayState(): the sole producer of NetState, so
// portal and telemetry stay as decoupled from the drivers as display is.
// Uses the same freshness window, for the same reason -- a stale fix must
// stop being published to HA and stop showing as a position on the captive
// page, rather than pinning the tracker to where it last had sky.
static NetState buildNetState() {
    NetState s{};

    bool haveFreshFix = everAccepted && (millis() - lastAcceptedAtMs) < DEFAULT_GPS_FRESH_MS;

    s.have_fix     = haveFreshFix;
    s.lat          = lastFix.lat;
    s.lon          = lastFix.lon;
    s.alt_m        = lastFix.alt_m;
    s.speed_kmh    = lastFix.speed_kmh;
    s.course_deg   = lastFix.course_deg;
    s.hdop         = lastFix.hdop;
    s.sats_used    = lastFix.sats;
    s.sats_in_view = gpsReader.satsInView();
    s.utc_unix     = lastFix.utc_unix;

    bool fromCompass = false;
    s.heading_deg = mag_sensor::arbitratedHeading(lastFix.speed_kmh, lastFix.course_deg,
                                                   haveFreshFix, &fromCompass);
    s.heading_from_compass = fromCompass;

    EnvReading env = env_sensor::last();
    s.temp_c    = env.temp_c;
    s.hum_pct   = env.hum_pct;
    s.env_valid = env.valid;

    s.accel_g        = accel_sensor::magnitudeG();
    s.cadence_moving = (cadence.state() == CadenceState::MOVING);

    s.receiving_nmea = gpsReader.receiving();
    s.uptime_s       = millis() / 1000;
    s.accepted_fixes = gateCounters[static_cast<uint8_t>(GateResult::ACCEPT)];
    s.emitted_fixes  = emitCount;

    return s;
}

// Multi-character serial command parser for "cal" (D-09), consulted BEFORE
// the existing single-char hotkey dispatch in loop()'s Serial.available()
// block (Pitfall 5 in 01.1-RESEARCH.md). Returns true when the character was
// consumed by this parser and must NOT also reach the single-char scheme;
// false when it should fall through unchanged. Fixed 8-byte buffer with an
// explicit bound check before every write (ASVS V5 control for threat
// T-01.1-01) -- overflow-prone input is discarded, never written past the
// buffer's declared size.
static bool calHandleChar(char c) {
    static const uint32_t kCalInactivityMs = 3000;   // distinct keystroke bursts never merge into one command
    static char     buf[8];
    static uint8_t  len = 0;
    static uint32_t lastCharMs = 0;

    uint32_t now = millis();
    if (len > 0 && (now - lastCharMs) > kCalInactivityMs) {
        len = 0;   // stale fragment from an earlier, unrelated keystroke burst
    }

    if (c == '\r' || c == '\n') {
        bool matched = (len == 3 && buf[0] == 'c' && buf[1] == 'a' && buf[2] == 'l');
        len = 0;
        if (matched) {
            if (mag_sensor::calActive()) {
                mag_sensor::calFinish();
            } else {
                mag_sensor::calStart();
            }
        }
        return true;   // Enter never falls through to the single-char dispatch
    }

    // Explicit bound check BEFORE any write, reserving room for a trailing
    // NUL -- the buffer must never be written past its declared size,
    // whatever the input.
    if (len >= sizeof(buf) - 1) {
        len = 0;
        return false;
    }

    buf[len] = c;
    len++;

    static const char kLiteral[] = "cal";
    bool isPrefix = true;
    for (uint8_t i = 0; i < len; i++) {
        if (buf[i] != kLiteral[i]) { isPrefix = false; break; }
    }

    if (isPrefix) {
        lastCharMs = now;
        return true;
    }

    len = 0;
    return false;
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
                // Re-runs the address probe + U8g2 begin() sequence so D-13
                // recovery actually re-initialises the OLED, not just flips
                // the flag. begin() already reports its own real result via
                // i2c_bus::setOnline() -- do not override it here.
                display::begin();
                break;
            case I2cModule::ACCEL:
                // Re-runs the WHO_AM_I + sleep-exit sequence so D-13 recovery
                // actually re-initialises the chip, not just flips the flag.
                // begin() already reports its own real result via
                // i2c_bus::setOnline() -- do not override it here.
                accel_sensor::begin();
                break;
            case I2cModule::MAG:
                // Re-runs address detection + the DFRobot_QMC5883 begin() sequence
                // so D-13 recovery actually re-initialises the chip, not just flips
                // the flag. begin() already reports its own real result via
                // i2c_bus::setOnline() -- do not override it here.
                mag_sensor::begin();
                break;
            case I2cModule::ENV:
                // Re-runs the Adafruit HTU21DF begin() sequence so D-13 recovery
                // actually re-initialises the chip, not just flips the flag.
                // begin() already reports its own real result via
                // i2c_bus::setOnline() -- do not override it here.
                env_sensor::begin();
                break;
            default:
                break;
        }

        // Only report "recovered" when the driver's own begin() actually
        // succeeded (i2c_bus::online(m) reflects that real result now, per
        // i2c_bus.h's setOnline() contract) -- a WHO_AM_I mismatch, missing
        // chip, or failed library begin() must NOT be reported as recovered
        // (CR-01). clearReinit() always runs, success or not, per contract.
        if (i2c_bus::online(m)) {
            Serial.printf("[I2C] %s recovered addr=0x%02X\n", i2c_bus::moduleName(m), i2c_bus::boundAddr(m));
        }
        i2c_bus::clearReinit(m);
    }
}

void loop() {
    Fix  fix;
    bool haveFix = gpsReader.poll(fix);

    if (Serial.available()) {
        char c = (char)Serial.read();
        if (!calHandleChar(c)) {
            if (c == 'r') {
                gpsReader.setRawEcho(!gpsReader.rawEcho());
                Serial.printf("\n[gps_tracker] raw echo: %s\n", gpsReader.rawEcho() ? "ON" : "OFF");
            } else {
                simHandleCommand(c);
            }
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

    // Compass + env poll (D-08/D-09/D-14). Both blocks are independent and
    // may coincide on the same iteration -- acceptable because the combined
    // blocking cost (a sub-millisecond compass read plus at most one
    // alternated HTU21D read, ~50ms) stays well inside the ~266ms GPS UART
    // buffer budget.
    if (millis() - lastMagPollMs >= DEFAULT_MAG_POLL_MS) {
        lastMagPollMs = millis();
        mag_sensor::poll();
    }

    if (millis() - lastEnvPollMs >= DEFAULT_HTU21_POLL_MS) {
        lastEnvPollMs = millis();
        env_sensor::poll();
    }

    // Display render (D-01/D-02/D-03/D-04). Positioned after the GPS/simTick
    // pass and every peripheral poll above, so the frame reflects this
    // iteration's fix + sensor state, never a half-processed one. Renders on
    // the DEFAULT_DISPLAY_REFRESH_MS timer, but also anticipates the render
    // to this iteration when a fresh GPS snapshot just arrived and at least
    // 80% of the interval has already elapsed -- without this, a free-running
    // 1 Hz timer and a 1 Hz fix that drift out of phase would always show the
    // previous fix, up to a full second late (D-02).
    {
        bool renderDue = (millis() - lastDisplayMs) >= DEFAULT_DISPLAY_REFRESH_MS;
        if (!renderDue && haveFix &&
            (millis() - lastDisplayMs) >= (DEFAULT_DISPLAY_REFRESH_MS * 4) / 5) {
            renderDue = true;
        }
        if (renderDue) {
            lastDisplayMs = millis();
            display::render(buildDisplayState());
        }
    }

    // Peripheral work always runs after the GPS pass above (poll/gate/
    // cadence/simTick already processed this iteration's fix snapshot) and
    // before the LED/[HEALTH] block -- the GPS pipeline never waits on I2C.
    i2cRecoverTick();

    // Network last, for the same reason I2C runs after the GPS pass: neither
    // portal nor telemetry may delay the UART drain. Both self-throttle and
    // return immediately when there is nothing to do, and neither ever waits
    // on an association or a broker socket (see telemetry.h).
    {
        NetState net = buildNetState();
        portal::tick(net);
        if (portal::configDirty()) {
            telemetry::reload(portal::creds());
        }
        telemetry::tick(net);
    }

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

        bool hdgFromCompass = false;
        float hdgDeg = mag_sensor::arbitratedHeading(lastFix.speed_kmh, lastFix.course_deg,
                                                      everAccepted, &hdgFromCompass);
        EnvReading envReading = env_sensor::last();

        Serial.printf(
            "[HEALTH] bytes=%lu ok=%lu bad=%lu sats=%u hdop=%.1f rx=%s"
            " accept=%lu no_fix=%lu stale=%lu null_island=%lu time=%lu hdop_rej=%lu sats_rej=%lu warmup=%lu jump=%lu"
            " cadence=%s since_emit=%lus emit=%lu suppress=%lu"
            " disp=%s accel=%s mag=%s env=%s i2c_drops=%lu"
            " accel_g=%.2f accel_wakes=%lu"
            " hdg=%.0f hdg_src=%s mag_chip=%s temp=%.1fC hum=%.0f%%"
            " sats_view=%u disp_addr=0x%02X"
            " ap=%s ap_ch=%u portal_polls=%lu sta=%s ip=%s sta_fails=%lu mqtt=%s pub=%lu\n",
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
            (unsigned long)accelWakeCount,
            hdgDeg, hdgFromCompass ? "mag" : "gps", mag_sensor::chipName(),
            envReading.temp_c, envReading.hum_pct,
            gpsReader.satsInView(), display::addr(),
            portal::apSsid(), (unsigned)portal::apChannel(), (unsigned long)portal::polls(),
            telemetry::wifiConnected() ? "up" : "down", telemetry::staIp(),
            (unsigned long)telemetry::staFailCount(),
            telemetry::mqttConnected() ? "up" : "down",
            (unsigned long)telemetry::published());

        if (mag_sensor::calActive()) {
            float xMin, xMax, yMin, yMax;
            mag_sensor::calRange(&xMin, &xMax, &yMin, &yMax);
            Serial.printf("[CAL] active %lus x=[%.0f..%.0f] y=[%.0f..%.0f]\n",
                          (unsigned long)mag_sensor::calElapsedS(), xMin, xMax, yMin, yMax);
        }
    }
}
