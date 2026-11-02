#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "comm_config.h"

// Cadeia do mic para voz: passa-alta 100 Hz (DC + ronco) → noise gate → AGC → limitador.
// Entrada: amostras de 24 bit do INMP441 (−26 dBFS @ 94 dB SPL). Saída: int16.
// Voz a 10–30 cm dá ~−50..−35 dBFS; o AGC sobe até COMM_MIC_MAX_GAIN_DB para ~−6 dBFS.
struct VoiceDsp {
    float x1 = 0, y1 = 0;           // estado do passa-alta
    float gain = 1.0f;
    uint16_t gate_hold = 0;         // blocos restantes com o gate aberto

    void reset() { x1 = y1 = 0; gain = 1.0f; gate_hold = 0; }

    void process(const int32_t *in, int16_t *out, size_t n) {
        const float A = 0.9615f;                         // e^(−2π·100/16000)
        const float FS = 8388608.0f;                     // 2^23
        static float buf[AUDIO_FRAME];
        float pk = 0, sq = 0;
        for (size_t i = 0; i < n; i++) {
            float x = in[i] / FS;
            float y = A * (y1 + x - x1);
            x1 = x; y1 = y;
            buf[i] = y;
            float a = fabsf(y);
            if (a > pk) pk = a;
            sq += y * y;
        }
        float rms = sqrtf(sq / n);

        // Gate: abre acima de ~−62 dBFS (voz próxima), segura 300 ms (15 blocos de 20 ms).
        if (rms > 8e-4f) gate_hold = 15;
        else if (gate_hold) gate_hold--;
        bool open = gate_hold > 0;

        // AGC: alvo de pico 0,5 (−6 dBFS). Desce rápido (sem estourar), sobe devagar.
        const float GMAX = powf(10.0f, COMM_MIC_MAX_GAIN_DB / 20.0f);
        if (open && pk > 1e-6f) {
            float want = 0.5f / pk;
            if (want > GMAX) want = GMAX;
            if (want < gain) gain = want;                // ataque imediato
            else gain += (want - gain) * 0.02f;          // ~1 s para subir
        }

        float g = open ? gain : 0.0f;                    // gate fechado = silêncio
        for (size_t i = 0; i < n; i++) {
            float v = buf[i] * g;
            if (v > 0.95f) v = 0.95f;                    // limitador duro (rede de segurança)
            if (v < -0.95f) v = -0.95f;
            out[i] = (int16_t)(v * 32767.0f);
        }
    }
};
