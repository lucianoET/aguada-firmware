// firmware/gps_tracker/src/mag_sensor.cpp
//
// See mag_sensor.h for the contract. Uses DFRobot_QMC5883 only for chip
// detection (isHMC()/isQMC()/isVCM()) and the raw X/Y/Z read (readRaw()) --
// heading is computed locally via atan2f on the hard-iron-corrected axes
// because the library's own getHeadingDegrees() neither applies calibration
// offsets nor returns a value (it mutates an internal field). Register
// addresses/behavior cross-checked against the installed
// DFRobot_QMC5883.h/.cpp source (01.1-RESEARCH.md, Code Examples).

#include "mag_sensor.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Preferences.h>
#include <DFRobot_QMC5883.h>

#include "gps_config.h"
#include "i2c_bus.h"

namespace {

// Anti-degenerate-capture floor (raw sensor units, not gauss/uT): a
// calibration capture must swing at least this much in BOTH X and Y before
// calFinish() accepts it -- rejects a capture where the module was barely
// rotated. Local to this file, not a build_flag: it is a sanity floor on the
// calibration procedure itself, not a tunable behavior.
constexpr float kCalMinRangeCounts = 10.0f;

DFRobot_QMC5883 *s_compass   = nullptr;
uint8_t          s_boundAddr = 0;
MagChip          s_chip      = MagChip::NONE;

float s_headingDeg = 0.0f;

MagCalibration s_cal = {0.0f, 0.0f, 0.0f, 0.0f, false};

bool     s_calActive  = false;
bool     s_calSeeded  = false;
uint32_t s_calStartMs = 0;
float    s_calXMin = 0.0f, s_calXMax = 0.0f;
float    s_calYMin = 0.0f, s_calYMax = 0.0f;

float normalizeDeg(float deg) {
    while (deg < 0.0f)    deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return deg;
}

void loadCalibration() {
    Preferences prefs;
    prefs.begin("magcal", true);   // read-only
    s_cal.xMin  = prefs.getFloat("xMin", 0.0f);
    s_cal.xMax  = prefs.getFloat("xMax", 0.0f);
    s_cal.yMin  = prefs.getFloat("yMin", 0.0f);
    s_cal.yMax  = prefs.getFloat("yMax", 0.0f);
    s_cal.valid = prefs.getBool("set", false);
    prefs.end();
}

void saveCalibration(float xMin, float xMax, float yMin, float yMax) {
    Preferences prefs;
    prefs.begin("magcal", false);
    prefs.putFloat("xMin", xMin);
    prefs.putFloat("xMax", xMax);
    prefs.putFloat("yMin", yMin);
    prefs.putFloat("yMax", yMax);
    prefs.putBool("set", true);
    prefs.end();

    s_cal.xMin  = xMin;
    s_cal.xMax  = xMax;
    s_cal.yMin  = yMin;
    s_cal.yMax  = yMax;
    s_cal.valid = true;
}

}  // namespace

bool mag_sensor::begin() {
    uint8_t addr = 0;
    if (i2c_bus::present(DEFAULT_MAG_ADDR_HMC)) {
        addr = DEFAULT_MAG_ADDR_HMC;
    } else if (i2c_bus::present(DEFAULT_MAG_ADDR_QMC)) {
        addr = DEFAULT_MAG_ADDR_QMC;
    } else if (i2c_bus::present(DEFAULT_MAG_ADDR_VCM)) {
        addr = DEFAULT_MAG_ADDR_VCM;
    }

    if (addr == 0) {
        s_chip = MagChip::NONE;
        i2c_bus::setOnline(I2cModule::MAG, false);
        return false;
    }

    if (s_compass == nullptr || s_boundAddr != addr) {
        delete s_compass;
        s_compass   = new DFRobot_QMC5883(&Wire, addr);
        s_boundAddr = addr;
    }

    bool ok = s_compass->begin();
    i2c_bus::restoreClock();   // library begin() may have changed the shared clock (Pitfall 2)

    if (!ok) {
        s_chip = MagChip::NONE;
        i2c_bus::setOnline(I2cModule::MAG, false);
        return false;
    }

    if (s_compass->isHMC()) {
        s_chip = MagChip::HMC5883L;
    } else if (s_compass->isQMC()) {
        s_chip = MagChip::QMC5883L;
    } else if (s_compass->isVCM()) {
        s_chip = MagChip::VCM5883L;
    } else {
        s_chip = MagChip::NONE;
    }

    i2c_bus::bind(I2cModule::MAG, addr);
    i2c_bus::setOnline(I2cModule::MAG, s_chip != MagChip::NONE);

    loadCalibration();

    Serial.printf("[MAG] chip=%s addr=0x%02X\n", mag_sensor::chipName(), addr);

    return s_chip != MagChip::NONE;
}

bool mag_sensor::poll() {
    // Re-init on recovery is driven solely by main.cpp::i2cRecoverTick()
    // (the only path i2c_bus::reinitDue() is ever actually observed true --
    // it consumes and clears the flag within the same loop() iteration it
    // was set, before this poll() would next run per WR-02). This branch
    // just yields last-known-value while offline.
    if (!i2c_bus::online(I2cModule::MAG)) {
        return false;
    }

    i2c_bus::restoreClock();

    // Cheap presence check ahead of the library's own (blocking-until-available)
    // readRaw() -- lets a genuinely disconnected module report through
    // i2c_bus::reportError() (D-13) rather than silently reusing stale data.
    Wire.beginTransmission(s_boundAddr);
    if (Wire.endTransmission() != 0) {
        i2c_bus::reportError(I2cModule::MAG);
        return false;
    }

    sVector_t v = s_compass->readRaw();

    float xMid = (s_cal.xMax + s_cal.xMin) * 0.5f;   // zero when no calibration saved yet
    float yMid = (s_cal.yMax + s_cal.yMin) * 0.5f;

    float xCorr = static_cast<float>(v.XAxis) - xMid;
    float yCorr = static_cast<float>(v.YAxis) - yMid;

    // D-10: X/Y only -- no tilt compensation, Z is read but never used here.
    float heading = atan2f(yCorr, xCorr) * (180.0f / PI);
    heading += DEFAULT_MAG_DECLINATION;
    s_headingDeg = normalizeDeg(heading);

    i2c_bus::reportOk(I2cModule::MAG);

    if (s_calActive) {
        if (!s_calSeeded) {
            s_calXMin = s_calXMax = static_cast<float>(v.XAxis);
            s_calYMin = s_calYMax = static_cast<float>(v.YAxis);
            s_calSeeded = true;
        } else {
            if (v.XAxis < s_calXMin) s_calXMin = v.XAxis;
            if (v.XAxis > s_calXMax) s_calXMax = v.XAxis;
            if (v.YAxis < s_calYMin) s_calYMin = v.YAxis;
            if (v.YAxis > s_calYMax) s_calYMax = v.YAxis;
        }

        if (mag_sensor::calElapsedS() >= static_cast<uint32_t>(DEFAULT_MAG_CAL_TIMEOUT_S)) {
            Serial.println("[CAL] timeout -- auto-finishing capture");
            mag_sensor::calFinish();
        }
    }

    return true;
}

MagChip mag_sensor::chip() {
    return s_chip;
}

const char *mag_sensor::chipName() {
    switch (s_chip) {
        case MagChip::HMC5883L: return "HMC5883L";
        case MagChip::QMC5883L: return "QMC5883L";
        case MagChip::VCM5883L: return "VCM5883L";
        default:                return "none";
    }
}

float mag_sensor::headingDeg() {
    return s_headingDeg;
}

const char *mag_sensor::cardinal(float deg) {
    static const char *kNames[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    int idx = static_cast<int>(roundf(normalizeDeg(deg) / 45.0f)) % 8;
    if (idx < 0) idx += 8;
    return kNames[idx];
}

void mag_sensor::calStart() {
    s_calActive  = true;
    s_calSeeded  = false;
    s_calStartMs = millis();
    Serial.println("[CAL] capture started -- rotate the module slowly through one full "
                    "horizontal turn, then send 'cal' again to save");
}

bool mag_sensor::calFinish() {
    if (!s_calActive) {
        return false;
    }

    float xRange = s_calXMax - s_calXMin;
    float yRange = s_calYMax - s_calYMin;

    if (!s_calSeeded || xRange < kCalMinRangeCounts || yRange < kCalMinRangeCounts) {
        Serial.printf("[CAL] rejected -- insufficient rotation (xRange=%.0f yRange=%.0f, min=%.0f); "
                      "previous calibration kept\n",
                      xRange, yRange, kCalMinRangeCounts);
        s_calActive = false;
        return false;
    }

    saveCalibration(s_calXMin, s_calXMax, s_calYMin, s_calYMax);
    Serial.printf("[CAL] saved xMin=%.0f xMax=%.0f yMin=%.0f yMax=%.0f\n",
                  s_calXMin, s_calXMax, s_calYMin, s_calYMax);
    s_calActive = false;
    return true;
}

void mag_sensor::calAbort() {
    s_calActive = false;
}

bool mag_sensor::calActive() {
    return s_calActive;
}

uint32_t mag_sensor::calElapsedS() {
    if (!s_calActive) return 0;
    return (millis() - s_calStartMs) / 1000;
}

MagCalibration mag_sensor::calibration() {
    return s_cal;
}

void mag_sensor::calRange(float *xMin, float *xMax, float *yMin, float *yMax) {
    if (xMin) *xMin = s_calXMin;
    if (xMax) *xMax = s_calXMax;
    if (yMin) *yMin = s_calYMin;
    if (yMax) *yMax = s_calYMax;
}

float mag_sensor::arbitratedHeading(float gpsSpeedKmh, float gpsCourseDeg, bool haveGpsFix, bool *fromCompass) {
    if (!i2c_bus::online(I2cModule::MAG)) {
        if (fromCompass) *fromCompass = false;
        return gpsCourseDeg;
    }

    if (!haveGpsFix) {
        if (fromCompass) *fromCompass = true;
        return s_headingDeg;
    }

    if (gpsSpeedKmh < DEFAULT_GPS_STATIONARY_KMH) {
        if (fromCompass) *fromCompass = true;
        return s_headingDeg;
    }

    if (fromCompass) *fromCompass = false;
    return gpsCourseDeg;
}
