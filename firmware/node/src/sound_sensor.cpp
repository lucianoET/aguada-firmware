#include "sound_sensor.h"
#include <Arduino.h>
#include <math.h>
#include <esp_log.h>

static const char *TAG = "sound";

static uint8_t s_pin   = 1;
static bool    s_ready = false;

// Number of samples per measurement window. ~1024 raw analogReads on the C3 take
// roughly 40–60 ms — long enough to capture the audio envelope, short enough to
// not disturb the captive portal / ESP-NOW loop (runs once per local tick).
#define SOUND_SAMPLES   1024
// Calibration offset for the relative "dB-like" scale. Tune after wiring so a
// quiet room reads ~35–40. Overridable via build flag.
#ifndef SOUND_DB_OFFSET
#define SOUND_DB_OFFSET 25.0f
#endif

void sound_init(uint8_t adc_pin) {
    s_pin = adc_pin;
    analogReadResolution(12);
    analogSetPinAttenuation(s_pin, ADC_11db);   // full range (~0–3.1 V)
    s_ready = true;
    ESP_LOGI(TAG, "KY-037/038 sound sensor on GPIO%d (ADC1, A0)", s_pin);
}

SoundReading sound_read(void) {
    SoundReading r = {};
    if (!s_ready) return r;

    uint32_t sum   = 0;
    uint64_t sumsq = 0;
    uint16_t vmin  = 4095, vmax = 0;

    for (int i = 0; i < SOUND_SAMPLES; i++) {
        uint16_t v = analogRead(s_pin);
        sum   += v;
        sumsq += (uint64_t)v * v;
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
    }

    float mean = (float)sum / SOUND_SAMPLES;
    float var  = (float)((double)sumsq / SOUND_SAMPLES) - mean * mean;
    if (var < 0.0f) var = 0.0f;
    float rms  = sqrtf(var);

    r.mean = (uint16_t)(mean + 0.5f);
    r.rms  = (uint16_t)(rms + 0.5f);
    r.vpp  = vmax - vmin;

    // Relative "dB-like" level (uncalibrated): 20*log10(rms) + offset.
    float db = 20.0f * log10f(rms + 1.0f) + SOUND_DB_OFFSET;
    if (db < 0.0f)   db = 0.0f;
    if (db > 120.0f) db = 120.0f;
    r.db = (uint8_t)(db + 0.5f);

    r.valid = true;
    return r;
}
