#pragma once
// firmware/gps_tracker/src/net_state.h
//
// NetState is the single frame the two network consumers share: portal
// (renders it as JSON for the captive page) and telemetry (publishes it to
// MQTT). It exists for the same reason DisplayState does -- main.cpp's
// buildNetState() is the only producer, so neither network module ever
// reaches into gps_reader/mag_sensor/env_sensor/cadence directly.
//
// Keep this struct plain data: no methods, no pointers into driver state.
// Anything a consumer needs must be a value copied at build time, because
// portal serves it from the HTTP handler and telemetry publishes it on its
// own timer -- both run at different moments than the loop() tick that
// produced it.

#include <stdint.h>
#include <stdbool.h>

struct NetState {
    // --- position (only meaningful when have_fix) ---
    bool     have_fix;
    double   lat;
    double   lon;
    float    alt_m;
    float    speed_kmh;
    float    course_deg;
    float    hdop;
    uint8_t  sats_used;
    uint8_t  sats_in_view;
    uint32_t utc_unix;        // 0 when GPS date/time is not yet valid

    // --- heading arbiter (compass vs GPS course) ---
    float    heading_deg;
    bool     heading_from_compass;

    // --- environment ---
    float    temp_c;
    float    hum_pct;
    bool     env_valid;

    // --- motion ---
    float    accel_g;
    bool     cadence_moving;

    // --- health ---
    bool     receiving_nmea;
    uint32_t uptime_s;
    uint32_t accepted_fixes;
    uint32_t emitted_fixes;
};
