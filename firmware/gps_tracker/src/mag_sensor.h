#pragma once
// firmware/gps_tracker/src/mag_sensor.h
//
// Compass driver — auto-detects HMC5883L (0x1E), QMC5883L (0x0D, the common
// GY-271 clone, D-11), or VCM5883L (0x0C) via DFRobot_QMC5883, computes
// heading locally from the raw X/Y axes with hard-iron offsets + magnetic
// declination applied (D-10, flat-mount assumed — no tilt compensation),
// persists calibration offsets in NVS (D-09), and arbitrates the displayed
// heading source by GPS speed (D-08) using the existing
// DEFAULT_GPS_STATIONARY_KMH threshold — no second, competing constant.
//
// Every function here is safe to call from loop() on every iteration: no
// blocking wait, no delay(), no sleep. Failure/retry policy is NOT
// reimplemented here — every read reports success/error to i2c_bus (D-13),
// which owns the offline-after-N-errors and scheduled-retry policy shared by
// all four Fase 01.1 peripherals.

#include <stdint.h>
#include <stdbool.h>

enum class MagChip : uint8_t { NONE, HMC5883L, QMC5883L, VCM5883L };

struct MagCalibration {
    float xMin;
    float xMax;
    float yMin;
    float yMax;
    bool  valid;
};

namespace mag_sensor {

// Probes i2c_bus::present() in order HMC/QMC/VCM to pick the responding
// address, (re)constructs the DFRobot_QMC5883 instance only when the
// detected address changed, calls its begin(), restores the shared I2C
// clock (the library's begin() may have changed it), classifies chip() from
// the library's isHMC()/isQMC()/isVCM() predicates, loads any persisted
// calibration from NVS, and reports the result via i2c_bus. Returns false
// (chip() == MagChip::NONE) when no candidate address responds.
bool begin();

// If offline, checks i2c_bus::reinitDue(MAG) and re-attempts begin() when
// due, then returns false. If online, does exactly one raw read via the
// library's readRaw(), computes heading locally from the corrected X/Y axes
// (offsets + DEFAULT_MAG_DECLINATION), updates the calibration capture
// window when calActive(), and auto-finishes an expired capture. Never
// blocks (beyond the library's own bus wait).
bool poll();

// Detected chip, or MagChip::NONE when no compass responds.
MagChip chip();

// "HMC5883L" | "QMC5883L" | "VCM5883L" | "none".
const char *chipName();

// Last computed heading in [0, 360), hard-iron-corrected and
// declination-adjusted. Offsets default to zero (degraded precision) when no
// calibration has been saved yet.
float headingDeg();

// One of "N"/"NE"/"E"/"SE"/"S"/"SW"/"W"/"NW" for the given heading.
const char *cardinal(float deg);

// Begins a hard-iron calibration capture: resets the min/max accumulators
// (seeded from the next sample, not literals), marks calActive() true, and
// stamps the start time.
void calStart();

// Validates the captured X/Y amplitude against a minimum anti-degenerate
// floor; if insufficient, rejects (keeps the previous calibration, logs the
// reason) and returns false. Otherwise persists the four floats + the "set"
// marker to NVS, updates the in-memory calibration, and returns true. Always
// ends the capture (calActive() becomes false) regardless of outcome.
bool calFinish();

// Discards the in-progress capture without validating or persisting.
void calAbort();

// True while a calibration capture is in progress.
bool calActive();

// Seconds elapsed since calStart(), or 0 when not active.
uint32_t calElapsedS();

// Currently loaded/saved calibration (valid == false when none saved yet).
MagCalibration calibration();

// D-08: below DEFAULT_GPS_STATIONARY_KMH (or with no GPS fix), returns the
// compass heading with *fromCompass = true; at/above it, returns
// gpsCourseDeg with *fromCompass = false. With the compass offline, always
// returns gpsCourseDeg with *fromCompass = false, regardless of speed/fix.
float arbitratedHeading(float gpsSpeedKmh, float gpsCourseDeg, bool haveGpsFix, bool *fromCompass);

}  // namespace mag_sensor
