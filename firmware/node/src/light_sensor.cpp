#include "light_sensor.h"
#include <Arduino.h>
#include <esp_log.h>

static const char *TAG = "light";

static uint8_t s_pin  = 3;
static bool    s_ready = false;

// Breakout load resistor (SparkFun-style TEMT6000 boards use 10 kΩ).
#define TEMT6000_LOAD_OHM   10000.0f
// Rough conversion: ~2 lux per µA of photocurrent (TEMT6000 typical, uncalibrated).
#define TEMT6000_LUX_PER_UA 2.0f
// Approx. full-scale of the ADC with 11 dB attenuation on the C3 (mV).
#define ADC_FULLSCALE_MV    3100.0f

void light_init(uint8_t adc_pin) {
    s_pin = adc_pin;
    analogReadResolution(12);
    analogSetPinAttenuation(s_pin, ADC_11db);   // full range (~0–3.1 V)
    s_ready = true;
    ESP_LOGI(TAG, "TEMT6000 light sensor on GPIO%d (ADC1)", s_pin);
}

LightReading light_read(void) {
    LightReading r = {};
    if (!s_ready) return r;

    const int N = 8;
    uint32_t raw_sum = 0, mv_sum = 0;
    for (int i = 0; i < N; i++) {
        raw_sum += analogRead(s_pin);
        mv_sum  += analogReadMilliVolts(s_pin);   // calibrated by the core
        delay(2);
    }
    r.raw = raw_sum / N;
    r.mv  = mv_sum / N;

    // lux estimate: I = V / R_load ; lux ≈ I[µA] * k
    float volts     = r.mv / 1000.0f;
    float microamps = (volts / TEMT6000_LOAD_OHM) * 1e6f;
    float lux       = microamps * TEMT6000_LUX_PER_UA;
    if (lux > 65535.0f) lux = 65535.0f;
    r.lux = (uint16_t)(lux + 0.5f);

    int pct = (int)(r.mv * 100.0f / ADC_FULLSCALE_MV + 0.5f);
    if (pct > 100) pct = 100;
    if (pct < 0)   pct = 0;
    r.pct = (uint8_t)pct;

    r.valid = true;
    return r;
}
