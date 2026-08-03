// firmware/gps_tracker/src/i2c_bus.cpp
//
// Implements the shared I2C bus policy declared in i2c_bus.h: boot scan,
// per-module online/offline state, consecutive-error counting, and
// round-robin retry scheduling (D-12/D-13). See i2c_bus.h for the contract
// planos 02/03/04 code against.
//
// Timing discipline (Pitfall 1/3 in 01.1-RESEARCH.md): nothing in this file
// blocks or sleeps. begin()'s full-bus scan is the ONLY complete sweep and
// it runs exactly once, before loop() starts. tick() does at most one
// address probe per call, round-robin across modules, so a single loop()
// iteration can never accumulate more than one I2C transaction here.

#include "i2c_bus.h"

#include <Arduino.h>
#include <Wire.h>

#include "gps_config.h"

namespace {

constexpr size_t kModuleCount = static_cast<size_t>(I2cModule::COUNT);

bool s_present[128] = {false};
uint8_t s_foundCount = 0;

bool s_online[kModuleCount] = {false, false, false, false};
uint16_t s_errorCount[kModuleCount] = {0, 0, 0, 0};
uint32_t s_offlineEvents[kModuleCount] = {0, 0, 0, 0};
uint8_t s_boundAddr[kModuleCount] = {0, 0, 0, 0};
uint32_t s_lastRetryMs[kModuleCount] = {0, 0, 0, 0};
bool s_reinitPending[kModuleCount] = {false, false, false, false};
uint8_t s_candidateCursor[kModuleCount] = {0, 0, 0, 0};
uint8_t s_retryCursor = 0;

const char *kModuleNames[kModuleCount] = {"display", "accel", "mag", "env"};

inline size_t idxOf(I2cModule m) { return static_cast<size_t>(m); }

bool probeAddr(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

// Candidate addresses for a module that has not (yet) bound to a specific
// address (e.g. it was absent during the boot scan). Returns 0 and sets
// *hasMore=false when idx is out of range for that module.
uint8_t candidateAddr(I2cModule m, uint8_t idx, bool *hasMore) {
    switch (m) {
        case I2cModule::DISPLAY: {
            static const uint8_t addrs[] = {DEFAULT_OLED_ADDR, DEFAULT_OLED_ADDR_ALT};
            if (idx < 2) {
                *hasMore = (idx + 1) < 2;
                return addrs[idx];
            }
            break;
        }
        case I2cModule::ACCEL: {
            static const uint8_t addrs[] = {DEFAULT_ACCEL_ADDR};
            if (idx < 1) {
                *hasMore = false;
                return addrs[idx];
            }
            break;
        }
        case I2cModule::MAG: {
            static const uint8_t addrs[] = {DEFAULT_MAG_ADDR_HMC, DEFAULT_MAG_ADDR_QMC, DEFAULT_MAG_ADDR_VCM};
            if (idx < 3) {
                *hasMore = (idx + 1) < 3;
                return addrs[idx];
            }
            break;
        }
        case I2cModule::ENV: {
            static const uint8_t addrs[] = {DEFAULT_ENV_ADDR};
            if (idx < 1) {
                *hasMore = false;
                return addrs[idx];
            }
            break;
        }
        default:
            break;
    }
    *hasMore = false;
    return 0;
}

// Single-probe retry attempt for module m: probes its bound address if one
// is known, otherwise the next candidate address in round-robin order (one
// address per call). Returns true and rebinds the module if it answered.
bool retryProbe(I2cModule m) {
    size_t idx = idxOf(m);
    uint8_t addr;

    if (s_boundAddr[idx] != 0) {
        addr = s_boundAddr[idx];
    } else {
        bool hasMore = false;
        addr = candidateAddr(m, s_candidateCursor[idx], &hasMore);
        if (addr == 0) {
            s_candidateCursor[idx] = 0;
            return false;
        }
        s_candidateCursor[idx] = hasMore ? static_cast<uint8_t>(s_candidateCursor[idx] + 1) : 0;
    }

    if (!probeAddr(addr)) {
        return false;
    }

    s_boundAddr[idx] = addr;
    return true;
}

}  // namespace

namespace i2c_bus {

void begin() {
    Wire.begin(DEFAULT_I2C_SDA_PIN, DEFAULT_I2C_SCL_PIN);
    Wire.setClock(DEFAULT_I2C_CLOCK_HZ);

    Serial.printf("[I2C] scan sda=%u scl=%u clk=%lu\n",
                  static_cast<unsigned>(DEFAULT_I2C_SDA_PIN),
                  static_cast<unsigned>(DEFAULT_I2C_SCL_PIN),
                  static_cast<unsigned long>(DEFAULT_I2C_CLOCK_HZ));

    s_foundCount = 0;
    for (uint16_t addr = 0x08; addr <= 0x77; addr++) {
        if (probeAddr(static_cast<uint8_t>(addr))) {
            s_present[addr] = true;
            s_foundCount++;
            Serial.printf("[I2C] found 0x%02X\n", static_cast<unsigned>(addr));
        }
    }

    if (s_present[DEFAULT_OLED_ADDR]) {
        bind(I2cModule::DISPLAY, DEFAULT_OLED_ADDR);
        setOnline(I2cModule::DISPLAY, true);
    } else if (s_present[DEFAULT_OLED_ADDR_ALT]) {
        bind(I2cModule::DISPLAY, DEFAULT_OLED_ADDR_ALT);
        setOnline(I2cModule::DISPLAY, true);
    }

    if (s_present[DEFAULT_ACCEL_ADDR]) {
        bind(I2cModule::ACCEL, DEFAULT_ACCEL_ADDR);
        setOnline(I2cModule::ACCEL, true);
    }

    if (s_present[DEFAULT_MAG_ADDR_HMC]) {
        bind(I2cModule::MAG, DEFAULT_MAG_ADDR_HMC);
        setOnline(I2cModule::MAG, true);
    } else if (s_present[DEFAULT_MAG_ADDR_QMC]) {
        bind(I2cModule::MAG, DEFAULT_MAG_ADDR_QMC);
        setOnline(I2cModule::MAG, true);
    } else if (s_present[DEFAULT_MAG_ADDR_VCM]) {
        bind(I2cModule::MAG, DEFAULT_MAG_ADDR_VCM);
        setOnline(I2cModule::MAG, true);
    }

    if (s_present[DEFAULT_ENV_ADDR]) {
        bind(I2cModule::ENV, DEFAULT_ENV_ADDR);
        setOnline(I2cModule::ENV, true);
    }

    Serial.printf("[I2C] modules disp=%s accel=%s mag=%s env=%s\n",
                  online(I2cModule::DISPLAY) ? "on" : "off",
                  online(I2cModule::ACCEL) ? "on" : "off",
                  online(I2cModule::MAG) ? "on" : "off",
                  online(I2cModule::ENV) ? "on" : "off");
}

void tick() {
    uint32_t now = millis();

    for (uint8_t i = 0; i < kModuleCount; i++) {
        uint8_t idx = static_cast<uint8_t>((s_retryCursor + i) % kModuleCount);
        if (s_online[idx]) continue;
        if (now - s_lastRetryMs[idx] < static_cast<uint32_t>(DEFAULT_I2C_RETRY_INTERVAL_MS)) continue;

        s_lastRetryMs[idx] = now;
        s_retryCursor = static_cast<uint8_t>((idx + 1) % kModuleCount);

        I2cModule m = static_cast<I2cModule>(idx);
        if (retryProbe(m)) {
            s_reinitPending[idx] = true;
            Serial.printf("[I2C] %s responding again addr=0x%02X\n", kModuleNames[idx], s_boundAddr[idx]);
        }
        return;  // at most one probe per tick() call
    }
}

bool present(uint8_t addr) { return s_present[addr]; }

uint8_t foundCount() { return s_foundCount; }

void restoreClock() { Wire.setClock(DEFAULT_I2C_CLOCK_HZ); }

void bind(I2cModule m, uint8_t addr) { s_boundAddr[idxOf(m)] = addr; }

uint8_t boundAddr(I2cModule m) { return s_boundAddr[idxOf(m)]; }

void setOnline(I2cModule m, bool ok) {
    size_t idx = idxOf(m);
    s_online[idx] = ok;
    if (ok) {
        s_errorCount[idx] = 0;
    }
}

bool online(I2cModule m) { return s_online[idxOf(m)]; }

void reportOk(I2cModule m) { s_errorCount[idxOf(m)] = 0; }

void reportError(I2cModule m) {
    size_t idx = idxOf(m);
    if (!s_online[idx]) return;  // already offline; nothing new to report

    s_errorCount[idx]++;
    if (s_errorCount[idx] >= static_cast<uint16_t>(DEFAULT_I2C_ERROR_THRESHOLD)) {
        s_online[idx] = false;
        s_offlineEvents[idx]++;
        s_lastRetryMs[idx] = millis();
        Serial.printf("[I2C] %s OFFLINE after %u consecutive errors\n", kModuleNames[idx], s_errorCount[idx]);
    }
}

uint16_t errorCount(I2cModule m) { return s_errorCount[idxOf(m)]; }

uint32_t offlineEvents(I2cModule m) { return s_offlineEvents[idxOf(m)]; }

bool reinitDue(I2cModule m) { return s_reinitPending[idxOf(m)]; }

void clearReinit(I2cModule m) { s_reinitPending[idxOf(m)] = false; }

const char *moduleName(I2cModule m) { return kModuleNames[idxOf(m)]; }

}  // namespace i2c_bus
