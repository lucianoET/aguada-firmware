#pragma once
// firmware/gps_tracker/src/i2c_bus.h
//
// Shared I2C bus manager for Fase 01.1's four bench peripherals (OLED
// SSD1306, MPU6050 accel, HMC5883L/QMC5883L/VCM5883L compass, HTU21D
// temp/hum). Owns the boot-time scan, per-module online/offline state,
// consecutive-error counting, and scheduled re-init retry (D-12/D-13) so
// each driver plugs into ONE policy instead of reinventing its own.
//
// This header is a CONTRACT: planos 02/03/04 code against these exact
// names/signatures. Nothing in this file may block or sleep -- every wait
// is expressed as a millis()-delta comparison spread across loop()
// iterations, because this module shares the superloop with the
// time-critical GPS UART consumer (gps_reader).

#include <stdint.h>
#include <stdbool.h>

// Arduino.h (pulled in transitively by any .cpp that also includes it)
// defines a legacy `DISPLAY` macro (0x1, an old Print-class output-mode
// constant) that collides with the enumerator name below. Undefine it here
// so `I2cModule::DISPLAY` compiles regardless of include order; Arduino.h's
// own include guard means this can't be redefined afterwards in the same
// translation unit.
#include <Arduino.h>
#undef DISPLAY

enum class I2cModule : uint8_t {
    DISPLAY = 0,
    ACCEL   = 1,
    MAG     = 2,
    ENV     = 3,
    COUNT   = 4
};

namespace i2c_bus {

// Initialise Wire on DEFAULT_I2C_SDA_PIN/DEFAULT_I2C_SCL_PIN at
// DEFAULT_I2C_CLOCK_HZ, scan addresses 0x08..0x77 once, log what was found,
// and seed each module's online flag from the scan result. Zero modules
// found is a valid state (D-12) -- this never blocks/aborts.
void begin();

// Called once per loop() iteration. Does bus work ONLY when at least one
// offline module's retry timer (DEFAULT_I2C_RETRY_INTERVAL_MS) has expired;
// otherwise returns immediately without touching the bus. At most one probe
// per call, round-robin across modules, so a single loop() iteration never
// accumulates more than one I2C transaction here.
void tick();

// Boot-scan result for a specific address.
bool present(uint8_t addr);

// How many addresses answered during the boot scan.
uint8_t foundCount();

// Re-assert DEFAULT_I2C_CLOCK_HZ. Drivers call this after any library
// .begin() (which may have silently changed the bus clock) and before each
// read, mirroring firmware/node/src/ani_sensor.cpp's restore-after-init
// pattern (RESEARCH.md Pitfall 2).
void restoreClock();

// Record/query the address a module was actually found at (OLED can be
// 0x3C or 0x3D; the compass can be 0x1E/0x0D/0x0C).
void bind(I2cModule m, uint8_t addr);
uint8_t boundAddr(I2cModule m);

// Driver reports its own begin() result. Passing true clears the
// consecutive-error counter.
void setOnline(I2cModule m, bool ok);
bool online(I2cModule m);

// Driver reports a successful read -- clears the consecutive-error counter.
void reportOk(I2cModule m);

// Driver reports a failed read -- increments the consecutive-error counter;
// at DEFAULT_I2C_ERROR_THRESHOLD marks the module offline, bumps its
// accumulated offline-event counter, arms the retry timer, and logs
// "[I2C] %s OFFLINE after %u consecutive errors" (D-13).
void reportError(I2cModule m);

// Current consecutive-error count for a module.
uint16_t errorCount(I2cModule m);

// Accumulated offline events since boot for a module.
uint32_t offlineEvents(I2cModule m);

// True once tick() has confirmed the module's address responds again and
// the driver has not yet attempted re-init.
bool reinitDue(I2cModule m);

// Driver calls this after attempting re-init, success or not.
void clearReinit(I2cModule m);

// "display", "accel", "mag", "env".
const char *moduleName(I2cModule m);

}  // namespace i2c_bus
