// GPS Tracker — bench harness (Fase 1)
// Wires GpsReader -> FixGate -> Cadence -> serial output. Every snapshot
// returned by poll() goes through FixGate::evaluate() before anything else
// happens with it; only the GateResult::ACCEPT branch may forward a fix
// downstream, and cadence's single call site lives inside that branch
// (Phase 2's flash logger attaches after cadence next). LED onboard: solid
// = most recent evaluation was ACCEPT, blinking = bytes arriving but the
// last result was a rejection, dark = no data at all.

#include <Arduino.h>
#include "gps_config.h"
#include "gps_types.h"
#include "gps_reader.h"
#include "fix_gate.h"
#include "cadence.h"

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
static uint32_t   gateCounters[kGateResultCount] = {0};

static GateResult lastPrintedReason = GateResult::ACCEPT;
static bool       havePrintedReason = false;
static GateResult lastResult        = GateResult::REJECT_NO_FIX;   // no data yet

static Fix       lastFix{};
static bool      everAccepted    = false;
static uint32_t  lastAcceptedAtMs = 0;

static inline void ledWrite(bool on) {
    digitalWrite(LED_PIN, LED_ACTIVE_LOW ? !on : on);
}

static void printRejectIfNew(GateResult result, float hdop, uint8_t sats, uint32_t ageMs, uint8_t quality) {
    if (havePrintedReason && result == lastPrintedReason) return;   // dedup consecutive same-reason
    Serial.printf("[REJECT] reason=%s hdop=%.1f sats=%u age=%lums q=%u\n",
                  gateResultName(result), hdop, sats, (unsigned long)ageMs, quality);
    lastPrintedReason = result;
    havePrintedReason = true;
}

static void printEmit(CadenceAction action, const Fix &f, CadenceState state) {
    const char *prefix = (action == CadenceAction::EMIT_HEARTBEAT) ? "[EMIT] hb=1" : "[EMIT]";
    Serial.printf(
        "%s t=%lu lat=%.6f lon=%.6f alt=%.1fm spd=%.1fkm/h crs=%.0f hdop=%.1f sats=%u q=%u age=%lums state=%s\n",
        prefix, (unsigned long)f.utc_unix, f.lat, f.lon, f.alt_m,
        f.speed_kmh, f.course_deg, f.hdop, f.sats, f.fix_quality,
        (unsigned long)f.age_ms, cadenceStateName(state));
}

static void printSuppress(CadenceAction action, const Fix &f, CadenceState state) {
    Serial.printf("[SUPPRESS] reason=%s spd=%.1fkm/h state=%s\n",
                  cadenceActionName(action), f.speed_kmh, cadenceStateName(state));
}

static void printStateChange(CadenceState oldState, CadenceState newState, float speedKmh, uint32_t elapsedS) {
    Serial.printf("[STATE] %s -> %s spd=%.1fkm/h elapsed_in_prev_state=%lus\n",
                  cadenceStateName(oldState), cadenceStateName(newState),
                  speedKmh, (unsigned long)elapsedS);
}

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    gpsReader.begin();
    Serial.println();
    Serial.printf("[gps_tracker] bench harness -- UART%d RX=%d TX=%d @ %d\n",
                  GPS_UART_NUM, GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);
    Serial.println("[gps_tracker] 'r' = toggle raw NMEA echo");
}

void loop() {
    Fix  fix;
    bool haveFix = gpsReader.poll(fix);

    if (Serial.available() && Serial.read() == 'r') {
        gpsReader.setRawEcho(!gpsReader.rawEcho());
        Serial.printf("\n[gps_tracker] raw echo: %s\n", gpsReader.rawEcho() ? "ON" : "OFF");
    }

    if (haveFix) {
        lastFix = fix;

        GateResult result = fixGate.evaluate(fix);
        gateCounters[static_cast<uint8_t>(result)]++;
        lastResult = result;

        if (result == GateResult::ACCEPT) {
            everAccepted     = true;
            lastAcceptedAtMs = fix.mono_ms;
            havePrintedReason = false;   // any later rejection always prints its first line

            // --- single downstream hand-off point -----------------------
            // Nothing outside this GateResult::ACCEPT branch may see, print,
            // or forward a gated fix.
            CadenceState  prevCadenceState = cadence.state();
            CadenceAction cadenceAction    = cadence.onGatedFix(fix);
            // --------------------------------------------------------------

            if (cadence.stateChanged()) {
                printStateChange(prevCadenceState, cadence.state(),
                                  cadence.lastTransitionSpeedKmh(),
                                  cadence.lastTransitionElapsedS());
            }

            if (cadenceAction == CadenceAction::EMIT || cadenceAction == CadenceAction::EMIT_HEARTBEAT) {
                emitCount++;
                printEmit(cadenceAction, fix, cadence.state());
            } else {
                suppressCount++;
                printSuppress(cadenceAction, fix, cadence.state());
            }
        } else {
            printRejectIfNew(result, fix.hdop, fix.sats, fix.age_ms, fix.fix_quality);
        }
    } else {
        // No new snapshot this tick. TinyGPSPlus's location validity/updated
        // flags are a one-way latch (01-RESEARCH.md Pattern 1) — once a fix
        // is fully lost mid-session, location never commits again, so
        // poll() alone can never surface that transition. This watchdog
        // reuses fix_gate's own DEFAULT_GPS_FRESH_MS window to detect a
        // frozen fix in real time (mitigates threat T-01-02), and covers
        // the symmetric cold-boot case where no fix has ever landed.
        if (!everAccepted) {
            gateCounters[static_cast<uint8_t>(GateResult::REJECT_NO_FIX)]++;
            printRejectIfNew(GateResult::REJECT_NO_FIX, lastFix.hdop, lastFix.sats, 0, lastFix.fix_quality);
            lastResult = GateResult::REJECT_NO_FIX;
        } else if ((millis() - lastAcceptedAtMs) >= DEFAULT_GPS_FRESH_MS) {
            uint32_t ageMs = millis() - lastAcceptedAtMs;
            gateCounters[static_cast<uint8_t>(GateResult::REJECT_STALE)]++;
            printRejectIfNew(GateResult::REJECT_STALE, lastFix.hdop, lastFix.sats, ageMs, lastFix.fix_quality);
            lastResult = GateResult::REJECT_STALE;
        }
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
        Serial.printf(
            "[HEALTH] bytes=%lu ok=%lu bad=%lu sats=%u hdop=%.1f rx=%s"
            " accept=%lu no_fix=%lu stale=%lu null_island=%lu time=%lu hdop_rej=%lu sats_rej=%lu warmup=%lu jump=%lu"
            " cadence=%s since_emit=%lus emit=%lu suppress=%lu\n",
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
            (unsigned long)suppressCount);
    }
}
