#include "ani_sensor.h"
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <ScioSense_ENS160.h>
#include <esp_log.h>

static const char *TAG = "ani";

static Adafruit_AHTX0   s_aht;
static ScioSense_ENS160 s_ens(ENS160_I2CADDR_0);

static bool s_aht_ok = false;
static bool s_ens_ok = false;

static uint8_t s_sda = 6;
static uint8_t s_scl = 7;

bool ani_init(uint8_t sda_pin, uint8_t scl_pin) {
    s_sda = sda_pin;
    s_scl = scl_pin;

    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(50000);
    delay(50);

    // Full I2C scan — log every device found on the bus
    ESP_LOGI(TAG, "I2C scan (sda=%d scl=%d):", sda_pin, scl_pin);
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            ESP_LOGI(TAG, "  found 0x%02X", addr);
        }
    }

    // AHT21 — I2C address 0x38 (fixed)
    // Pass &Wire explicitly; Adafruit_AHTX0 still calls Wire.begin() internally
    s_aht_ok = s_aht.begin(&Wire);
    if (!s_aht_ok) {
        ESP_LOGW(TAG, "AHT21 not found (sda=%d scl=%d)", sda_pin, scl_pin);
    } else {
        ESP_LOGI(TAG, "AHT21 OK");
    }

    // ENS160 — I2C address 0x52 (ADDR pin low)
    s_ens.begin();
    if (!s_ens.available()) {
        ESP_LOGW(TAG, "ENS160 not found");
        s_ens_ok = false;
    } else {
        s_ens.setMode(ENS160_OPMODE_STD);
        s_ens_ok = true;
        ESP_LOGI(TAG, "ENS160 OK (fw %d.%d.%d)",
                 s_ens.getMajorRev(), s_ens.getMinorRev(), s_ens.getBuild());
    }

    // Restore correct pins and clock after library Wire.begin() calls
    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(50000);

    return s_aht_ok && s_ens_ok;
}

void ani_status(bool *aht_ok, bool *ens_ok) {
    if (aht_ok) *aht_ok = s_aht_ok;
    if (ens_ok) *ens_ok = s_ens_ok;
}

AniReading ani_read(void) {
    AniReading r = {};

    if (!s_aht_ok || !s_ens_ok) {
        ESP_LOGW(TAG, "Sensor(s) not available: aht=%d ens=%d", s_aht_ok, s_ens_ok);
        return r;
    }

    // Ensure clock didn't drift after other library inits
    Wire.setClock(50000);

    // 1. Read AHT21 (temp + hum)
    static float s_last_temp = 25.0f;
    static float s_last_hum  = 50.0f;
    sensors_event_t hum_evt, temp_evt;
    bool aht_ok = s_aht.getEvent(&hum_evt, &temp_evt);
    if (!aht_ok) {
        ESP_LOGW(TAG, "AHT21 read failed — retrying");
        Wire.begin(s_sda, s_scl);
        Wire.setClock(50000);
        aht_ok = s_aht.getEvent(&hum_evt, &temp_evt);
    }
    if (aht_ok) {
        r.temp_c  = temp_evt.temperature;
        r.hum_pct = hum_evt.relative_humidity;
        s_last_temp = r.temp_c;
        s_last_hum  = r.hum_pct;
        ESP_LOGI(TAG, "AHT21 T=%.1fC H=%.0f%%", r.temp_c, r.hum_pct);
    } else {
        // AHT21 unavailable — use last known values and continue reading ENS160
        r.temp_c  = s_last_temp;
        r.hum_pct = s_last_hum;
        ESP_LOGW(TAG, "AHT21 read failed after recovery — using last T=%.1fC H=%.0f%%",
                 r.temp_c, r.hum_pct);
    }

    // 2. Feed T/H compensation to ENS160 (improves accuracy)
    s_ens.set_envdata(r.temp_c, r.hum_pct);

    // 3. Wait for ENS160 DATA_IS_READY (max 1 second)
    uint32_t deadline = millis() + 1000UL;
    while (!s_ens.available() && millis() < deadline) {
        delay(10);
    }
    if (!s_ens.available()) {
        ESP_LOGW(TAG, "ENS160 data not ready within 1s");
        return r;
    }

    s_ens.measure(true);

    r.eco2  = s_ens.geteCO2();
    r.tvoc  = s_ens.getTVOC();
    r.aqi   = s_ens.getAQI();
    r.valid = true;

    ESP_LOGI(TAG, "ANI eco2=%uppm tvoc=%uppb aqi=%u T=%.1fC H=%.0f%%",
             r.eco2, r.tvoc, r.aqi, r.temp_c, r.hum_pct);
    return r;
}
