// fix_gate evaluates the immutable Fix snapshot only — never the parser,
// the UART, or Serial. Its only library dependency is TinyGPSPlus's static
// distanceBetween() helper (great-circle distance, for the outlier/jump
// check) — no haversine hand-rolled here.

#include "fix_gate.h"
#include "gps_config.h"
#include <TinyGPSPlus.h>

const char *gateResultName(GateResult r) {
    switch (r) {
        case GateResult::ACCEPT:             return "ACCEPT";
        case GateResult::REJECT_NO_FIX:       return "REJECT_NO_FIX";
        case GateResult::REJECT_STALE:        return "REJECT_STALE";
        case GateResult::REJECT_NULL_ISLAND:  return "REJECT_NULL_ISLAND";
        case GateResult::REJECT_TIME:         return "REJECT_TIME";
        case GateResult::REJECT_HDOP:         return "REJECT_HDOP";
        case GateResult::REJECT_SATS:         return "REJECT_SATS";
        case GateResult::REJECT_WARMUP:       return "REJECT_WARMUP";
        case GateResult::REJECT_OUTLIER_JUMP: return "REJECT_OUTLIER_JUMP";
    }
    return "REJECT_UNKNOWN";
}

GateResult FixGate::evaluate(const Fix &f) {
    // 1. GGA fix quality floor. Fix quality 1 (standalone GPS) is the
    // accept floor — neither target module produces DGPS/SBAS values under
    // D-02, so requiring more would reject every real fix (Open Question 2).
    if (f.fix_quality < 1) {
        warmupStreak_ = 0;
        return GateResult::REJECT_NO_FIX;
    }

    // 2. Staleness. This is the single most important line in the module:
    // TinyGPSPlus's location validity flag latches true on the first fix
    // and is never cleared when lock is lost, so age_ms is the only signal
    // that a position has gone stale.
    if (f.age_ms >= DEFAULT_GPS_FRESH_MS) {
        warmupStreak_ = 0;
        return GateResult::REJECT_STALE;
    }

    // 3. Explicit null-island guard.
    if (f.lat == 0.0 && f.lon == 0.0) {
        warmupStreak_ = 0;
        return GateResult::REJECT_NULL_ISLAND;
    }

    // 4. No trustworthy UTC date yet — Phase 2 records must be timestamped.
    if (f.utc_unix == 0) {
        warmupStreak_ = 0;
        return GateResult::REJECT_TIME;
    }

    // 5. HDOP.
    if (f.hdop > DEFAULT_GPS_HDOP_MAX) {
        warmupStreak_ = 0;
        return GateResult::REJECT_HDOP;
    }

    // 6. Satellite count.
    if (f.sats < DEFAULT_GPS_SATS_MIN) {
        warmupStreak_ = 0;
        return GateResult::REJECT_SATS;
    }

    // 7. Warmup: require N consecutive checks-1-6-passing fixes before
    // trusting the stream for the outlier/accept decision below.
    if (warmupStreak_ < DEFAULT_GPS_WARMUP_FIXES) {
        warmupStreak_++;
        return GateResult::REJECT_WARMUP;
    }

    // 8. Implausible jump vs. the last accepted fix. One wild sample must
    // not poison the reference point, so the stored last-accepted position
    // is left untouched on rejection here.
    if (haveLastAccepted_) {
        double distM   = TinyGPSPlus::distanceBetween(lastLat_, lastLon_, f.lat, f.lon);
        double dtHours = (f.mono_ms - lastMonoMs_) / 3600000.0;
        if (dtHours > 0.0) {
            double impliedKmh = (distM / 1000.0) / dtHours;
            if (impliedKmh > DEFAULT_GPS_MAX_PLAUSIBLE_KMH) {
                return GateResult::REJECT_OUTLIER_JUMP;
            }
        }
    }

    lastLat_          = f.lat;
    lastLon_          = f.lon;
    lastMonoMs_       = f.mono_ms;
    haveLastAccepted_ = true;
    return GateResult::ACCEPT;
}

void FixGate::reset() {
    warmupStreak_     = 0;
    haveLastAccepted_ = false;
    lastLat_          = 0.0;
    lastLon_          = 0.0;
    lastMonoMs_       = 0;
}
