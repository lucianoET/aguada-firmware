#pragma once
// firmware/gps_tracker/include/gps_config.h
//
// Tunable GPS acquisition / gating / cadence constants. Every value here is
// #ifndef-guarded so platformio.ini `build_flags -D...` can override it
// without touching source — same shape as firmware/node/include/node_config.h.
//
// IMPORTANT: GPS_BENCH_SIM must be set to 0 before any networked build
// (Phase 3 onward, once WiFi/MQTT enter scope). It exists only to mark this
// phase as bench-only/offline and must never ship enabled once the tracker
// talks to a network.

// --- gps_reader (Task 1) -------------------------------------------------

#ifndef DEFAULT_GPS_FRESH_MS
#define DEFAULT_GPS_FRESH_MS 2500   // location.age() at/above this => stale (fix_gate REJECT_STALE)
#endif

#ifndef DEFAULT_GPS_MIN_UTC_YEAR
#define DEFAULT_GPS_MIN_UTC_YEAR 2024   // reject pre-lock default GPS dates when deriving utc_unix
#endif

#ifndef DEFAULT_GPS_HEALTH_PERIOD_MS
#define DEFAULT_GPS_HEALTH_PERIOD_MS 5000   // [HEALTH] line print period
#endif

#ifndef GPS_BENCH_SIM
#define GPS_BENCH_SIM 1
#endif

// --- fix_gate (Task 2) ---------------------------------------------------
// These four are research starting points (Assumptions A1, A2, A4 in
// 01-RESEARCH.md), not final values — tune against field data.

#ifndef DEFAULT_GPS_HDOP_MAX
#define DEFAULT_GPS_HDOP_MAX 5.0f   // reject above this HDOP (A1)
#endif

#ifndef DEFAULT_GPS_SATS_MIN
#define DEFAULT_GPS_SATS_MIN 4   // hard minimum satellites used (A2)
#endif

#ifndef DEFAULT_GPS_WARMUP_FIXES
#define DEFAULT_GPS_WARMUP_FIXES 3   // consecutive gate-passing fixes required post-cold-start (A4)
#endif

#ifndef DEFAULT_GPS_MAX_PLAUSIBLE_KMH
#define DEFAULT_GPS_MAX_PLAUSIBLE_KMH 200.0f   // implied-speed outlier gate vs last accepted fix
#endif

// --- cadence (Plan 02) ----------------------------------------------------
// These five are research starting points (Assumptions A3, A4 in
// 01-RESEARCH.md), not final values — pending a multi-hour stationary test
// in the real mounting location. Any field override belongs in
// platformio.ini build_flags, never by editing this header.

#ifndef DEFAULT_GPS_MOVING_KMH
#define DEFAULT_GPS_MOVING_KMH 5.0f   // upper hysteresis edge: at/above this is a "moving" vote (A3) —
                                       // stays clear of the bench-observed 2.6 km/h stationary jitter
#endif

#ifndef DEFAULT_GPS_STATIONARY_KMH
#define DEFAULT_GPS_STATIONARY_KMH 3.0f   // lower hysteresis edge: at/below this is a "stationary" vote (A3) —
                                            // the gap to DEFAULT_GPS_MOVING_KMH is what prevents flapping
#endif

#ifndef DEFAULT_GPS_DEBOUNCE_FIXES
#define DEFAULT_GPS_DEBOUNCE_FIXES 3   // consecutive confirming accepted fixes required before a state flip (A4)
#endif

#ifndef DEFAULT_GPS_CADENCE_MOVING_S
#define DEFAULT_GPS_CADENCE_MOVING_S 8   // emit interval while MOVING, seconds — mid-point of GPS-03's 5-10s range
#endif

#ifndef DEFAULT_GPS_STATIONARY_HEARTBEAT_S
#define DEFAULT_GPS_STATIONARY_HEARTBEAT_S 0   // 0 disables the stationary heartbeat (Phase 1 default: GPS-03
                                                 // only requires acceptance to pause when parked). The constant
                                                 // exists so Phase 2's flash logger can turn on a low-rate
                                                 // "still parked" record later without touching cadence logic.
#endif
