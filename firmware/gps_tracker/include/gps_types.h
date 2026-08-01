#pragma once
#include <stdint.h>

// Fix is the immutable snapshot that is this phase's contract with every
// downstream consumer (fix_gate this phase; Phase 2's flash logger next).
// gps_reader is the only module that ever populates one; everyone else only
// ever reads it.
struct Fix {
    double   lat;          // degrees, 0.0 when never fixed
    double   lon;          // degrees, 0.0 when never fixed
    float    speed_kmh;    // km/h, RMC/Doppler-derived
    float    course_deg;   // degrees, 0-360
    float    alt_m;        // metres
    float    hdop;         // dimensionless; sentinel 99.9f when unavailable so an
                            // absent HDOP is later rejected rather than silently accepted
    uint8_t  sats;         // satellites used in the fix (not in view)
    uint8_t  fix_quality;  // GGA field 6 (TinyGPSLocation::Quality), 0 = invalid
    uint32_t utc_unix;     // seconds since Unix epoch, derived from GPS date+time;
                            // 0 when the date or time is not yet valid
    uint32_t age_ms;       // parser-reported location age at snapshot time
    uint32_t mono_ms;      // millis() at snapshot time — all interval arithmetic uses
                            // this, never utc_unix, which can jump when the receiver
                            // first acquires date
};

// GateResult is fix_gate's decision over a Fix snapshot. ACCEPT is the only
// outcome that may be forwarded downstream (cadence in Plan 02, the flash
// logger in Phase 2); every REJECT_* variant carries a specific, named
// reason for the bench serial log.
enum class GateResult : uint8_t {
    ACCEPT,
    REJECT_NO_FIX,
    REJECT_STALE,
    REJECT_NULL_ISLAND,
    REJECT_TIME,
    REJECT_HDOP,
    REJECT_SATS,
    REJECT_WARMUP,
    REJECT_OUTLIER_JUMP,
};

// Returns the enum member name as a string, for serial output and
// per-reason health counters.
const char *gateResultName(GateResult r);

// CadenceState is the moving/stationary classification cadence maintains
// across accepted fixes (Plan 02). Movement is classified from the
// receiver's own speed field, never from position deltas.
enum class CadenceState : uint8_t {
    MOVING,
    STATIONARY,
};

// CadenceAction is cadence's pacing decision for a single gate-accepted
// fix: EMIT/EMIT_HEARTBEAT mean "forward this fix downstream"; the
// SUPPRESS_* variants mean "this fix was gated-accepted but cadence is
// holding it back" and carry a specific reason for the bench serial log.
enum class CadenceAction : uint8_t {
    EMIT,
    EMIT_HEARTBEAT,
    SUPPRESS_STATIONARY,
    SUPPRESS_INTERVAL,
};

// Returns the enum member name as a string, for serial output.
const char *cadenceStateName(CadenceState s);
const char *cadenceActionName(CadenceAction a);
