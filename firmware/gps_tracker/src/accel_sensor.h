#pragma once
// firmware/gps_tracker/src/accel_sensor.h
//
// Hand-rolled raw-I2C MPU6050 driver -- accel only (D-07: gyroscope
// registers are never touched). Reads acceleration magnitude and produces a
// debounced wake edge consumed by main.cpp to call Cadence::onAccelWake()
// (D-05/D-06). Every function here is safe to call from loop() on every
// iteration: no blocking wait, no delay(), no sleep -- this driver shares
// the superloop with the time-critical GPS UART consumer (gps_reader).
//
// Failure/retry policy is NOT reimplemented here: every read reports
// success/error to i2c_bus (D-13), which owns the offline-after-N-errors and
// scheduled-retry policy shared by all four Fase 01.1 peripherals.

#include <stdint.h>
#include <stdbool.h>

struct AccelReading {
    float magnitude_g;   // Euclidean magnitude of X/Y/Z acceleration, in g
    bool  valid;         // false when the last poll() failed or none has run yet
};

namespace accel_sensor {

// Probes WHO_AM_I (register 0x75, expects 0x68) then writes PWR_MGMT_1
// (register 0x6B) = 0x00 to bring the chip out of sleep. No other register
// is touched, so the accelerometer full-scale stays at its power-on-reset
// default (+/-2 g, 16384 LSB/g). Reports the combined result once via
// i2c_bus::setOnline(I2cModule::ACCEL, ok) and, on success, binds the
// address via i2c_bus::bind(). Returns the same ok value.
bool begin();

// If offline, checks i2c_bus::reinitDue(I2cModule::ACCEL) and re-attempts
// begin() when due, then returns false. If online, does exactly one 6-byte
// burst read of ACCEL_XOUT_H..ACCEL_ZOUT_H (register 0x3B), computes the
// magnitude, and feeds the debounced wake detector. On any I2C error,
// reports it via i2c_bus::reportError(), marks the last reading invalid,
// resets the debounce streak, and returns false without touching the wake
// detector. Never blocks.
bool poll();

// Most recent reading. magnitude_g is meaningless when valid == false.
AccelReading last();

// One-shot: true only once per confirmed debounce streak (see poll()).
// Consuming this call clears the pending flag, mirroring
// Cadence::stateChanged()'s style.
bool wakeEdge();

// Convenience accessor for the last magnitude_g, for [ACCEL]/[HEALTH] log lines.
float magnitudeG();

// Current consecutive-above-threshold sample count, for the [ACCEL] log line.
uint8_t debounceStreak();

}  // namespace accel_sensor
