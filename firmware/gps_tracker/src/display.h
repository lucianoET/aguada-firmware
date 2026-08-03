#pragma once
// firmware/gps_tracker/src/display.h
//
// OLED SSD1306 presentation layer (D-01..D-04, D-12/D-13). Renders one of
// two dense, single-screen layouts from a caller-supplied DisplayState: the
// full status screen once a GPS fix is available, or the acquisition screen
// (D-03) while waiting for one -- never both, never paginated, never gated
// behind a button.
//
// This module has NO dependency on accel_sensor/mag_sensor/env_sensor/
// gps_reader -- render() consumes only the DisplayState it is given, so the
// presentation layer stays decoupled from every driver (main.cpp's
// buildDisplayState() is the only place that reads the drivers).
//
// Every function here is safe to call from loop() on every iteration: no
// blocking wait, no delay(), no sleep beyond the single U8g2 sendBuffer()
// transfer render() performs per call. Failure/retry policy is NOT
// reimplemented here -- begin()/render() report through i2c_bus (D-13),
// which owns the offline-after-N-errors and scheduled-retry policy shared by
// all four Fase 01.1 peripherals.

#include <stdint.h>
#include <stdbool.h>
#include "gps_types.h"

// One fully-assembled display frame. main.cpp's buildDisplayState() is the
// only producer; display::render() is the only consumer.
struct DisplayState {
    bool        have_fix;             // false => render the D-03 acquisition screen instead
    uint8_t     sats_used;            // satellites used in the fix (Fix::sats)
    uint8_t     sats_in_view;         // GpsReader::satsInView() -- meaningful even without a fix
    float       hdop;
    float       speed_display;        // already converted to the display unit (D-04)
    const char *speed_unit;           // "km/h" | "kt"
    float       heading_deg;          // mag_sensor::arbitratedHeading() result
    const char *heading_cardinal;     // mag_sensor::cardinal() of heading_deg
    bool        heading_from_compass; // true => compass source, false => GPS course
    float       temp_c;
    float       hum_pct;
    bool        env_valid;            // false => temp_c/hum_pct are last-known values (D-14)
    float       accel_g;
    bool        cadence_moving;       // true => MOVING, false => STOPPED
    bool        receiving_nmea;       // GpsReader::receiving() -- distinguishes "no sky" from "no UART"
    uint32_t    uptime_s;
    bool        accel_online;         // i2c_bus::online(I2cModule::ACCEL)
    bool        mag_online;           // i2c_bus::online(I2cModule::MAG)
    bool        env_online;           // i2c_bus::online(I2cModule::ENV)
};

namespace display {

// Probes i2c_bus::present() first at DEFAULT_OLED_ADDR then
// DEFAULT_OLED_ADDR_ALT. With neither responding, marks the module offline
// via i2c_bus::setOnline() and returns false without touching the U8g2
// library. With an address found, configures U8g2's bus clock and 8-bit I2C
// address, calls the library's begin(), restores the shared I2C clock
// (Pitfall 2 -- the library's begin() may have changed it), binds the
// address via i2c_bus::bind(), and reports online.
bool begin();

// Returns immediately when the display is offline, handling
// i2c_bus::reinitDue() the same way the plan 02/03 drivers do (begin() then
// clearReinit()). When online: restores the shared I2C clock, clears the
// framebuffer, draws exactly one frame from `s`, and transfers it in exactly
// one sendBuffer() call -- no blocking wait of its own beyond that single
// I2C transfer (~25ms at 400kHz for the full 1024-byte buffer).
void render(const DisplayState &s);

// The I2C address the display was actually found at (0 when never found),
// for the [HEALTH] disp_addr= field.
uint8_t addr();

}  // namespace display
