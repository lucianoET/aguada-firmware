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
