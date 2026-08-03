// firmware/gps_tracker/src/env_sensor.cpp
//
// See env_sensor.h for the contract. Adafruit_HTU21DF's two conversion
// getters each internally block ~50ms (Pitfall 6 in 01.1-RESEARCH.md);
// alternating them across successive poll() calls halves the per-call
// blocking cost, following the last-known-value fallback shape of
// firmware/node/src/ani_sensor.cpp -- but deliberately WITHOUT its blocking
// `while (!available) delay(10)` wait loop.

#include "env_sensor.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_HTU21DF.h>

#include "gps_config.h"
#include "i2c_bus.h"

namespace {

Adafruit_HTU21DF s_htu;

EnvReading s_last          = {25.0f, 50.0f, false};   // last-known-value fallback seed
bool       s_readTempNext  = true;                    // flip-flop: alternate temp / humidity reads

}  // namespace

bool env_sensor::begin() {
    bool ok = s_htu.begin(&Wire);
    i2c_bus::restoreClock();   // library begin() may have changed the shared clock (Pitfall 2)

    i2c_bus::setOnline(I2cModule::ENV, ok);
    if (ok) {
        i2c_bus::bind(I2cModule::ENV, DEFAULT_ENV_ADDR);
    }
    return ok;
}

bool env_sensor::poll() {
    // Re-init on recovery is driven solely by main.cpp::i2cRecoverTick()
    // (the only path i2c_bus::reinitDue() is ever actually observed true --
    // it consumes and clears the flag within the same loop() iteration it
    // was set, before this poll() would next run per WR-02). This branch
    // just yields last-known-value while offline.
    if (!i2c_bus::online(I2cModule::ENV)) {
        return false;
    }

    i2c_bus::restoreClock();

    bool doTemp = s_readTempNext;
    s_readTempNext = !s_readTempNext;   // alternate regardless of this call's outcome

    bool ok;
    if (doTemp) {
        float t = s_htu.readTemperature();
        ok = !isnan(t);
        if (ok) {
            s_last.temp_c = t;
        }
    } else {
        float h = s_htu.readHumidity();
        ok = !isnan(h);
        if (ok) {
            s_last.hum_pct = h;
        }
    }

    if (ok) {
        i2c_bus::reportOk(I2cModule::ENV);
        s_last.valid = true;
    } else {
        i2c_bus::reportError(I2cModule::ENV);
        s_last.valid = false;   // last known temp_c/hum_pct are preserved above
    }

    return ok;
}

EnvReading env_sensor::last() {
    return s_last;
}
