#pragma once
// firmware/gps_tracker/src/env_sensor.h
//
// Adafruit HTU21DF (0x40) temperature/humidity wrapper for D-14: display and
// serial log only this phase -- no flash log, no network payload
// integration happens here. Alternates one of the two blocking library
// reads (temperature XOR humidity) per poll() call, halving the per-call
// blocking cost against the ~266ms GPS UART buffer budget (Pitfall 1/6 in
// 01.1-RESEARCH.md). A transient read failure never blanks the last known
// values -- only valid flips to false.
//
// Failure/retry policy is NOT reimplemented here: every read reports
// success/error to i2c_bus (D-13), which owns the offline-after-N-errors and
// scheduled-retry policy shared by all four Fase 01.1 peripherals.

#include <stdint.h>
#include <stdbool.h>

struct EnvReading {
    float temp_c;
    float hum_pct;
    bool  valid;   // false when the most recent poll()'s read failed (temp_c/hum_pct hold the last known values)
};

namespace env_sensor {

// Initialises the Adafruit HTU21DF driver, restores the shared I2C clock
// (the library's begin() may have changed it), and reports the result via
// i2c_bus.
bool begin();

// If offline, checks i2c_bus::reinitDue(ENV) and re-attempts begin() when
// due, then returns. If online, restores the shared I2C clock and performs
// exactly ONE of readTemperature()/readHumidity() per call, alternating via
// an internal flip-flop -- never both in the same call. A NAN result is
// treated as a failed read: reports i2c_bus::reportError(), leaves the
// corresponding field at its last known value, and marks valid false. A
// numeric result updates that field, reports i2c_bus::reportOk(), and marks
// valid true. Never blocks beyond the library's own internal delay.
bool poll();

// Last known reading. valid reflects only the most recent poll() call.
EnvReading last();

}  // namespace env_sensor
