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
