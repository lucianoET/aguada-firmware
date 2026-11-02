#pragma once
#include <Arduino.h>
#include <math.h>
#include "comm_config.h"

// Tons gerados no falante (gongo do PTT, sirene, bip). Comuns ao communicator e ao Yeti.

static float s_tone_phase = 0;

static inline void tone_fill(int16_t *out, size_t n, float hz, float amp) {
    const float step = 2.0f * (float)M_PI * hz / AUDIO_RATE_HZ;
    for (size_t i = 0; i < n; i++) {
        out[i] = (int16_t)(amp * 32767.0f * sinf(s_tone_phase));
        s_tone_phase += step;
        if (s_tone_phase > 2.0f * (float)M_PI) s_tone_phase -= 2.0f * (float)M_PI;
    }
}

// Gongo de anúncio: 2 tons (dó–lá) de 240 ms cada, com decaimento. bloco = índice de 20 ms.
static inline bool chime_block(int16_t *out, size_t n, int bloco) {
    if (bloco >= 24) return false;
    float hz = bloco < 12 ? 1046.5f : 880.0f;
    float env = 0.5f * expf(-(bloco % 12) / 5.0f);
    tone_fill(out, n, hz, env);
    return true;
}

// Sirene no falante: varredura 600↔1200 Hz em 1 s.
static inline void siren_block(int16_t *out, size_t n) {
    float t = (millis() % 1000) / 1000.0f;
    float hz = 600.0f + 600.0f * (t < 0.5f ? t * 2 : (1 - t) * 2);
    tone_fill(out, n, hz, 0.6f);
}
