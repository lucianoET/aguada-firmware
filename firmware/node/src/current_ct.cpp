#include "current_ct.h"
#include <Arduino.h>
#include <math.h>
#include <esp_log.h>

static const char *TAG = "ct";

#define CT_WINDOW_MS     200       // 12 ciclos @ 60 Hz (10 @ 50 Hz)
#define CT_BIAS_MIN_MV   1100      // janela de bias aceitável → TC presente
#define CT_BIAS_MAX_MV   2200
#ifndef CT_NOISE_MV
#define CT_NOISE_MV      4.0f      // ruído RMS do ADC, subtraído em quadratura
#endif
#define CT_MIN_AMPS      0.10f     // abaixo disto reporta 0 A
#define CT_CLIP_HI_MV    2450      // limite linear do ADC do C3 com 11 dB
#define CT_CLIP_LO_MV    80

static int8_t s_pin = -1;

void current_init(int8_t adc_pin) {
    s_pin = adc_pin;
    if (s_pin < 0) return;
    analogReadResolution(12);
    analogSetPinAttenuation(s_pin, ADC_11db);
    ESP_LOGI(TAG, "porta CT em GPIO%d (%.0f A/V)", s_pin, (double)CT_AMPS_PER_VOLT);
}

CurrentReading current_read(void) {
    CurrentReading r = {};
    if (s_pin < 0) return r;

    // Acumula em mV calibrados (eFuse) para a escala ficar correcta sem ajuste.
    double sum = 0, sumsq = 0;
    uint32_t n = 0, vmin = UINT32_MAX, vmax = 0, t0 = millis();
    while (millis() - t0 < CT_WINDOW_MS) {
        uint32_t mv = analogReadMilliVolts(s_pin);
        double v = mv;
        sum += v; sumsq += v * v; n++;
        if (mv < vmin) vmin = mv;
        if (mv > vmax) vmax = mv;
    }
    if (n < 100) return r;

    double mean = sum / n;
    double var  = sumsq / n - mean * mean;
    double vrms = var > 0 ? sqrt(var) : 0;
    r.bias_mv = (uint16_t)(mean + 0.5);
    r.present = (mean >= CT_BIAS_MIN_MV && mean <= CT_BIAS_MAX_MV);
    if (!r.present) return r;

    double sig = vrms * vrms - (double)CT_NOISE_MV * CT_NOISE_MV;
    double amps = (sig > 0 ? sqrt(sig) : 0) / 1000.0 * CT_AMPS_PER_VOLT;
    r.amps    = amps < CT_MIN_AMPS ? 0.0f : (float)amps;
    r.clipped = (vmax >= CT_CLIP_HI_MV) || (vmin <= CT_CLIP_LO_MV);
    r.valid   = true;
    if (r.clipped)
        ESP_LOGW(TAG, "TC saturado (min=%lu max=%lumV) — usar TC de maior faixa",
                 (unsigned long)vmin, (unsigned long)vmax);
    ESP_LOGI(TAG, "I=%.2fA (vrms=%.1fmV bias=%umV n=%lu)", r.amps, vrms, r.bias_mv, (unsigned long)n);
    return r;
}
