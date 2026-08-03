// firmware/gps_tracker/src/display.cpp
//
// See display.h for the contract. render() only ever reads from the
// DisplayState it is given -- no sensor-driver namespace call appears in
// this file, keeping the presentation layer decoupled from every driver
// (main.cpp's buildDisplayState() is the sole bridge).

#include "display.h"

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include "gps_config.h"
#include "i2c_bus.h"

namespace {

// Full-buffer, hardware-I2C SSD1306 128x64 driver (01.1-RESEARCH.md Standard
// Stack) -- transfers the whole 1024-byte frame in a single buffer-send call
// (~25ms at 400kHz), the "one transaction per render()" budget T-01.1-03
// depends on. Rotation/reset are fixed at construction (a U8g2 constructor
// requirement); the I2C address is resolved at runtime in begin() via
// setI2CAddress() because the OLED can answer at either DEFAULT_OLED_ADDR or
// DEFAULT_OLED_ADDR_ALT.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

uint8_t s_addr = 0;

void formatUptime(uint32_t uptimeS, char *buf, size_t bufLen) {
    uint32_t h   = uptimeS / 3600;
    uint32_t m   = (uptimeS % 3600) / 60;
    uint32_t sec = uptimeS % 60;
    snprintf(buf, bufLen, "%02lu:%02lu:%02lu",
             (unsigned long)h, (unsigned long)m, (unsigned long)sec);
}

// Compact offline-module marker letters for the bottom-right corner
// (D-12/D-13): one letter per module, present only while that module
// reports offline. Empty string when everything is online -- this is what
// answers "why isn't this field changing?" directly on the screen, with no
// serial monitor needed.
void formatOfflineMarkers(const DisplayState &s, char *buf, size_t bufLen) {
    uint8_t n = 0;
    if (!s.accel_online && (n + 1) < bufLen) buf[n++] = 'A';
    if (!s.mag_online   && (n + 1) < bufLen) buf[n++] = 'C';
    if (!s.env_online   && (n + 1) < bufLen) buf[n++] = 'E';
    buf[n] = '\0';
}

void drawRightAligned(u8g2_uint_t y, const char *s) {
    if (s[0] == '\0') return;
    u8g2_uint_t w = u8g2.getStrWidth(s);
    u8g2_uint_t x = (w < 128) ? static_cast<u8g2_uint_t>(128 - w) : 0;
    u8g2.drawStr(x, y, s);
}

}  // namespace

bool display::begin() {
    uint8_t addr = 0;
    if (i2c_bus::present(DEFAULT_OLED_ADDR)) {
        addr = DEFAULT_OLED_ADDR;
    } else if (i2c_bus::present(DEFAULT_OLED_ADDR_ALT)) {
        addr = DEFAULT_OLED_ADDR_ALT;
    }

    if (addr == 0) {
        i2c_bus::setOnline(I2cModule::DISPLAY, false);
        return false;
    }

    u8g2.setBusClock(DEFAULT_I2C_CLOCK_HZ);
    u8g2.setI2CAddress(static_cast<uint8_t>(addr << 1));   // U8g2 expects the 8-bit form

    bool ok = u8g2.begin();
    i2c_bus::restoreClock();   // library begin() may have changed the shared clock (Pitfall 2)

    if (!ok) {
        i2c_bus::setOnline(I2cModule::DISPLAY, false);
        return false;
    }

    s_addr = addr;
    i2c_bus::bind(I2cModule::DISPLAY, addr);
    i2c_bus::setOnline(I2cModule::DISPLAY, true);
    return true;
}

uint8_t display::addr() {
    return s_addr;
}

void display::render(const DisplayState &s) {
    // Re-init on recovery is driven solely by main.cpp::i2cRecoverTick()
    // (the only path i2c_bus::reinitDue() is ever actually observed true --
    // it consumes and clears the flag within the same loop() iteration it
    // was set, before this render() would next run per WR-02). This branch
    // just skips the frame while offline.
    if (!i2c_bus::online(I2cModule::DISPLAY)) {
        return;
    }

    i2c_bus::restoreClock();

    char line[24];
    char uptimeBuf[10];
    char offlineBuf[4];
    formatUptime(s.uptime_s, uptimeBuf, sizeof(uptimeBuf));
    formatOfflineMarkers(s, offlineBuf, sizeof(offlineBuf));

    u8g2.clearBuffer();

    // Rows 1-2: fix status (D-01) or acquisition status (D-03) -- the only
    // two rows whose content differs by have_fix.
    if (s.have_fix) {
        u8g2.setFont(u8g2_font_6x10_tf);
        snprintf(line, sizeof(line), "FIX sat=%u hdop=%.1f", s.sats_used, s.hdop);
        u8g2.drawStr(0, 9, line);

        u8g2.setFont(u8g2_font_9x15_tf);
        snprintf(line, sizeof(line), "%.0f %s", s.speed_display, s.speed_unit);
        u8g2.drawStr(0, 27, line);
    } else {
        u8g2.setFont(u8g2_font_6x10_tf);
        snprintf(line, sizeof(line), "NO FIX view=%u", s.sats_in_view);
        u8g2.drawStr(0, 9, line);

        snprintf(line, sizeof(line), "nmea=%s up=%s",
                 s.receiving_nmea ? "OK" : "NONE", uptimeBuf);
        u8g2.drawStr(0, 27, line);
    }

    // Rows 3-5: heading / temp+hum / cadence+accel -- identical in both fix
    // states, because D-03's whole point is proving these sensors are alive
    // while the GPS receiver is still cold.
    u8g2.setFont(u8g2_font_6x10_tf);

    snprintf(line, sizeof(line), "HDG %.0f %s %s",
             s.heading_deg, s.heading_cardinal, s.heading_from_compass ? "(C)" : "(G)");
    u8g2.drawStr(0, 37, line);

    snprintf(line, sizeof(line), "T=%.1fC H=%.0f%%%s",
             s.temp_c, s.hum_pct, s.env_valid ? "" : " old");
    u8g2.drawStr(0, 47, line);

    snprintf(line, sizeof(line), "%s acc=%.2fg",
             s.cadence_moving ? "MOVING" : "STOPPED", s.accel_g);
    u8g2.drawStr(0, 57, line);

    // Row 6: uptime (left) + compact offline-module markers (right,
    // D-12/D-13) -- always shown, regardless of fix state.
    snprintf(line, sizeof(line), "UP %s", uptimeBuf);
    u8g2.drawStr(0, 63, line);
    drawRightAligned(63, offlineBuf);

    u8g2.sendBuffer();
}
