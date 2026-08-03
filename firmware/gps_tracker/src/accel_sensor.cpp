// firmware/gps_tracker/src/accel_sensor.cpp
//
// See accel_sensor.h for the contract. Register addresses/scale factor
// cross-checked against 01.1-RESEARCH.md Pattern 4 (InvenSense MPU-6050
// register map). Accel only (D-07) -- registers other than WHO_AM_I (0x75),
// PWR_MGMT_1 (0x6B), and ACCEL_XOUT_H..ACCEL_ZOUT_H (0x3B) are never
// touched, so the gyroscope is never enabled or read.

#include "accel_sensor.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "gps_config.h"
#include "i2c_bus.h"

namespace {

constexpr uint8_t kWhoAmIReg   = 0x75;
constexpr uint8_t kWhoAmIValue = 0x68;
constexpr uint8_t kPwrMgmt1Reg = 0x6B;
constexpr uint8_t kAccelXoutH  = 0x3B;
constexpr float   kLsbPerG     = 16384.0f;   // reset-default full-scale (+/-2g)

AccelReading s_last{0.0f, false};
uint8_t      s_debounceStreak  = 0;
bool         s_wakeEdgePending = false;

}  // namespace

bool accel_sensor::begin() {
    i2c_bus::restoreClock();

    bool ok = false;
    do {
        Wire.beginTransmission(DEFAULT_ACCEL_ADDR);
        Wire.write(kWhoAmIReg);
        if (Wire.endTransmission(false) != 0) break;

        uint8_t got = Wire.requestFrom(static_cast<uint8_t>(DEFAULT_ACCEL_ADDR), static_cast<uint8_t>(1));
        if (got < 1 || !Wire.available() || Wire.read() != kWhoAmIValue) break;

        Wire.beginTransmission(DEFAULT_ACCEL_ADDR);
        Wire.write(kPwrMgmt1Reg);
        Wire.write((uint8_t)0x00);
        if (Wire.endTransmission() != 0) break;

        ok = true;
    } while (false);

    i2c_bus::setOnline(I2cModule::ACCEL, ok);
    if (ok) {
        i2c_bus::bind(I2cModule::ACCEL, DEFAULT_ACCEL_ADDR);
    }
    return ok;
}

bool accel_sensor::poll() {
    if (!i2c_bus::online(I2cModule::ACCEL)) {
        if (i2c_bus::reinitDue(I2cModule::ACCEL)) {
            begin();
            i2c_bus::clearReinit(I2cModule::ACCEL);
        }
        return false;
    }

    i2c_bus::restoreClock();

    Wire.beginTransmission(DEFAULT_ACCEL_ADDR);
    Wire.write(kAccelXoutH);
    if (Wire.endTransmission(false) != 0) {
        i2c_bus::reportError(I2cModule::ACCEL);
        s_last.valid = false;
        s_debounceStreak = 0;
        return false;
    }

    uint8_t got = Wire.requestFrom(static_cast<uint8_t>(DEFAULT_ACCEL_ADDR), static_cast<uint8_t>(6));
    if (got < 6 || Wire.available() < 6) {
        i2c_bus::reportError(I2cModule::ACCEL);
        s_last.valid = false;
        s_debounceStreak = 0;
        return false;
    }

    int16_t axRaw = (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();
    int16_t ayRaw = (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();
    int16_t azRaw = (static_cast<int16_t>(Wire.read()) << 8) | Wire.read();

    float gx = static_cast<float>(axRaw) / kLsbPerG;
    float gy = static_cast<float>(ayRaw) / kLsbPerG;
    float gz = static_cast<float>(azRaw) / kLsbPerG;
    float magnitude = sqrtf(gx * gx + gy * gy + gz * gz);

    i2c_bus::reportOk(I2cModule::ACCEL);
    s_last.magnitude_g = magnitude;
    s_last.valid = true;

    // Wake-edge debounce (D-05/D-06): a sustained deviation from the 1.0 g
    // baseline arms a one-shot edge after DEFAULT_ACCEL_WAKE_DEBOUNCE_SAMPLES
    // consecutive above-threshold samples; a single below-threshold sample
    // resets the streak, rejecting an isolated jolt.
    float deviation = fabsf(magnitude - 1.0f);
    if (deviation >= DEFAULT_ACCEL_WAKE_THRESHOLD_G) {
        if (s_debounceStreak < 255) s_debounceStreak++;
        if (s_debounceStreak >= DEFAULT_ACCEL_WAKE_DEBOUNCE_SAMPLES) {
            s_wakeEdgePending = true;
            s_debounceStreak  = 0;
        }
    } else {
        s_debounceStreak = 0;
    }

    return true;
}

AccelReading accel_sensor::last() {
    return s_last;
}

bool accel_sensor::wakeEdge() {
    bool edge = s_wakeEdgePending;
    s_wakeEdgePending = false;
    return edge;
}

float accel_sensor::magnitudeG() {
    return s_last.magnitude_g;
}

uint8_t accel_sensor::debounceStreak() {
    return s_debounceStreak;
}
