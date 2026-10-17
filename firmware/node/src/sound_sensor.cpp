#include "sound_sensor.h"
#include <Arduino.h>
#include <ESP_I2S.h>
#include <driver/gpio.h>
#include <math.h>
#include <esp_log.h>

static const char *TAG = "sound";

#define MIC_RATE_HZ      32000       // 32 kHz: filtro A preciso até 6,3 kHz (a 16 kHz erra >5 dB)
#define MIC_WINDOW       8192        // amostras na janela = 256 ms
#define MIC_FLUSH        3072        // descarte: anel DMA antigo + assentamento do filtro (96 ms)
#define MIC_CHUNK        512
// INMP441: -26 dBFS @ 94 dB SPL, com 0 dBFS = RMS de senoide de fundo de escala
// (AES17) = 2^23/√2. Com amplitudes medidas contra 2^23: dB SPL = 20log(x/2^23)
// + 3.01 + 120. Vale para o RMS e para o pico (94 dB RMS → 97 dB pico).
#define MIC_DBFS_TO_SPL  123.01
#ifndef MIC_DB_OFFSET
#define MIC_DB_OFFSET    0.0         // ajuste fino contra sonómetro, se houver
#endif
// Alinhamento: no C3 (ESP_I2S, formato Philips, slot 32 bit) o INMP441 chega 1 bit
// atrasado — a palavra de 24 bit ocupa os bits 31..9 e o bit 8 é sempre 0 (medido:
// bit 0 do valor >>8 nunca muda). >>9 devolve a amostra certa; >>8 lia o dobro (+6 dB).
// Perde-se o MSB, mas o bit 22 repete o sinal até ~114 dB SPL — fora da nossa faixa.
#ifndef MIC_SHIFT
#define MIC_SHIFT        9
#endif

// Ponderação A (IEC 61672) para fs = 32 kHz: transformada bilinear das constantes
// analógicas (20,6 / 107,7 / 737,9 / 12194 Hz), 3 secções biquad [b0 b1 b2 a0 a1 a2].
// Resposta: 31,5 Hz -39,5 · 63 Hz -26,2 · 1 kHz 0,0 · 4 kHz +0,9 · 6,3 kHz -0,7 · 8 kHz -2,6 dB.
// double: o C3 não tem FPU (custo ~0,1 s por janela, 1 vez a cada 8 s) e os pólos
// perto de z=1 pedem mais precisão do que float.
static const double A_SOS[3][6] = {
    {0.343458338682, 0.686916677365, 0.343458338682, 1.0, 0.179471731469, 0.008052525599},
    {1.000000000000, -2.000141613451, 1.000141623479, 1.0, -1.843990656139, 0.846816324094},
    {1.000000000000, -1.999858386549, 0.999858396575, 1.0, -1.991927118563, 0.991943411416},
};

typedef struct { double z1[3], z2[3]; } afilt_t;

// Direct Form II transposta, secção a secção.
static inline double a_weight(afilt_t *f, double x) {
    for (int s = 0; s < 3; s++) {
        const double *c = A_SOS[s];
        double y = c[0] * x + f->z1[s];
        f->z1[s] = c[1] * x - c[4] * y + f->z2[s];
        f->z2[s] = c[2] * x - c[5] * y;
        x = y;
    }
    return x;
}

static I2SClass s_i2s;
#ifdef AIRQ_DIAG
#define DIAG_N 4096
static int32_t s_dbg[DIAG_N];   // amostras cruas (já alinhadas) para análise espectral no PC
static size_t  s_dbg_n = 0;
#endif
static bool     s_ready = false;
static int32_t  s_buf[MIC_CHUNK];

void sound_init(int8_t sck_pin, int8_t ws_pin, int8_t sd_pin) {
    s_ready = false;
    if (sck_pin < 0 || ws_pin < 0 || sd_pin < 0) return;
    s_i2s.setPins(sck_pin, ws_pin, -1, sd_pin);
    s_i2s.setTimeout(100);
    if (!s_i2s.begin(I2S_MODE_STD, MIC_RATE_HZ, I2S_DATA_BIT_WIDTH_32BIT,
                     I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
        ESP_LOGW(TAG, "falha ao iniciar I2S (sck=%d ws=%d sd=%d)", sck_pin, ws_pin, sd_pin);
        return;
    }
    // Sem microfone o SD fica a flutuar; com pull-down lê zeros → detectável.
    gpio_pulldown_en((gpio_num_t)sd_pin);
    s_ready = true;
    ESP_LOGI(TAG, "INMP441 I2S sck=%d ws=%d sd=%d @ %d Hz, dB(A)", sck_pin, ws_pin, sd_pin, MIC_RATE_HZ);
}

typedef struct {
    afilt_t f;
    double  sumsq_a;     // Σ y² (ponderado A) — só fora do assentamento
    double  peak_a;      // max |y|
    double  sum_z, sumsq_z;   // nível Z (sem ponderação) — só diagnóstico
    int32_t vmin, vmax;  // amostras cruas — detecção de microfone ausente
    size_t  n;           // amostras contabilizadas
} acc_t;

// Lê `want` amostras e passa todas pelo filtro A; com `count`=false só assenta o
// filtro (descarte). Devolve nº lido.
static size_t read_samples(size_t want, acc_t *a, bool count) {
    size_t got = 0;
    while (got < want) {
        size_t n = want - got > MIC_CHUNK ? MIC_CHUNK : want - got;
        size_t bytes = s_i2s.readBytes((char *)s_buf, n * sizeof(int32_t));
        size_t k = bytes / sizeof(int32_t);
        if (k == 0) break;   // timeout
        for (size_t i = 0; i < k; i++) {
            int32_t v = s_buf[i] >> MIC_SHIFT;
            double  y = a_weight(&a->f, (double)v);
            if (!count) continue;
#ifdef AIRQ_DIAG
            if (s_dbg_n < DIAG_N) s_dbg[s_dbg_n++] = v;
#endif
            a->sumsq_a += y * y;
            if (fabs(y) > a->peak_a) a->peak_a = fabs(y);
            a->sum_z += v; a->sumsq_z += (double)v * v;
            if (v < a->vmin) a->vmin = v;
            if (v > a->vmax) a->vmax = v;
            a->n++;
        }
        got += k;
    }
    return got;
}

SoundReading sound_read(void) {
    SoundReading r = {};
    if (!s_ready) return r;

    acc_t a = {};
    a.vmin = INT32_MAX; a.vmax = INT32_MIN;
#ifdef AIRQ_DIAG
    s_dbg_n = 0;
#endif
    // 1. descarta o anel DMA antigo e deixa o filtro A assentar (pólo de 20 Hz ~8 ms)
    read_samples(MIC_FLUSH, &a, false);
    // 2. janela medida
    read_samples(MIC_WINDOW, &a, true);
    if (a.n < MIC_WINDOW / 2 || a.vmax - a.vmin < 4) {   // sem microfone (zeros) ou I2S parado
        ESP_LOGW(TAG, "INMP441 sem sinal (n=%u span=%ld)", (unsigned)a.n, (long)(a.vmax - a.vmin));
        return r;
    }

    const double FS = 8388608.0;                    // 2^23
    double rms_a = sqrt(a.sumsq_a / a.n);
    r.db_spl  = (float)(20.0 * log10(fmax(rms_a, 1.0) / FS) + MIC_DBFS_TO_SPL + MIC_DB_OFFSET);
    r.peak_db = (float)(20.0 * log10(fmax(a.peak_a, 1.0) / FS) + MIC_DBFS_TO_SPL + MIC_DB_OFFSET);
    r.valid   = true;

    double mean_z = a.sum_z / a.n;
    double var_z  = a.sumsq_z / a.n - mean_z * mean_z;
    double db_z   = 20.0 * log10(fmax(sqrt(fmax(var_z, 1.0)), 1.0) / FS) + MIC_DBFS_TO_SPL + MIC_DB_OFFSET;
    ESP_LOGI(TAG, "nível=%.1f dB(A)  pico=%.1f dB(A)  (Z=%.1f dB)", r.db_spl, r.peak_db, db_z);

#ifdef AIRQ_DIAG
    // despejo da janela crua (1 de cada 4 leituras) — linhas "MICRAW <idx> v,v,v…"
    static uint8_t s_dump = 0;
    if ((s_dump++ & 3) == 0) {
        for (size_t i = 0; i < s_dbg_n; i += 64) {
            char line[64 * 9 + 24];
            int n = snprintf(line, sizeof(line), "MICRAW %u ", (unsigned)i);
            for (size_t k = i; k < i + 64 && k < s_dbg_n; k++)
                n += snprintf(line + n, sizeof(line) - n, "%ld,", (long)s_dbg[k]);
            Serial.println(line);
        }
        Serial.println("MICRAW END");
    }
#endif
    return r;
}
