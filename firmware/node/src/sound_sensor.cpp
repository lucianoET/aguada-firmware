#include "sound_sensor.h"
#include <Arduino.h>
#include <ESP_I2S.h>
#include <driver/gpio.h>
#include <math.h>
#include <esp_log.h>

static const char *TAG = "sound";

#define MIC_RATE_HZ      16000
#define MIC_WINDOW       4096        // amostras na janela = 256 ms
#define MIC_FLUSH        1536        // amostras descartadas (anel DMA ~1440 frames)
#define MIC_CHUNK        512
// INMP441: -26 dBFS @ 94 dB SPL, com 0 dBFS = RMS de senoide de fundo de escala
// (AES17) = 2^23/√2. Com amplitudes medidas contra 2^23: dB SPL = 20log(x/2^23)
// + 3.01 + 120. Vale para o RMS e para o pico (94 dB RMS → 97 dB pico).
#define MIC_DBFS_TO_SPL  123.01f
#ifndef MIC_DB_OFFSET
#define MIC_DB_OFFSET    0.0f        // ajuste fino contra sonómetro, se houver
#endif

static I2SClass s_i2s;
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
    ESP_LOGI(TAG, "INMP441 I2S sck=%d ws=%d sd=%d @ %d Hz", sck_pin, ws_pin, sd_pin, MIC_RATE_HZ);
}

// Lê até `want` amostras; se `acc` != null acumula estatística. Devolve nº lido.
static size_t read_samples(size_t want, double *sum, double *sumsq,
                           int32_t *vmin, int32_t *vmax) {
    size_t got = 0;
    while (got < want) {
        size_t n = want - got > MIC_CHUNK ? MIC_CHUNK : want - got;
        size_t bytes = s_i2s.readBytes((char *)s_buf, n * sizeof(int32_t));
        size_t k = bytes / sizeof(int32_t);
        if (k == 0) break;   // timeout
        if (sum) {
            for (size_t i = 0; i < k; i++) {
                int32_t v = s_buf[i] >> 8;   // 24 bit úteis, alinhados à esquerda
                *sum += v; *sumsq += (double)v * v;
                if (v < *vmin) *vmin = v;
                if (v > *vmax) *vmax = v;
            }
        }
        got += k;
    }
    return got;
}

SoundReading sound_read(void) {
    SoundReading r = {};
    if (!s_ready) return r;

    read_samples(MIC_FLUSH, nullptr, nullptr, nullptr, nullptr);
    double sum = 0, sumsq = 0;
    int32_t vmin = INT32_MAX, vmax = INT32_MIN;
    size_t n = read_samples(MIC_WINDOW, &sum, &sumsq, &vmin, &vmax);
    if (n < MIC_WINDOW / 2 || vmax - vmin < 4) {   // sem microfone (zeros) ou I2S parado
        ESP_LOGW(TAG, "INMP441 sem sinal (n=%u span=%ld)", (unsigned)n, (long)(vmax - vmin));
        return r;
    }

    double mean = sum / n;                          // remove offset DC do MEMS
    double var  = sumsq / n - mean * mean;
    double rms  = var > 1.0 ? sqrt(var) : 1.0;
    double pk   = fmax(fabs((double)vmax - mean), fabs((double)vmin - mean));
    const double FS = 8388608.0;                    // 2^23
    r.db_spl  = (float)(20.0 * log10(rms / FS)) + MIC_DBFS_TO_SPL + MIC_DB_OFFSET;
    r.peak_db = (float)(20.0 * log10(fmax(pk, 1.0) / FS)) + MIC_DBFS_TO_SPL + MIC_DB_OFFSET;
    r.valid   = true;
    ESP_LOGI(TAG, "nível=%.1f dB SPL  pico=%.1f dB", r.db_spl, r.peak_db);
    return r;
}
