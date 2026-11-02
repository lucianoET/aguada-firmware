#include "audio_io.h"
#include "comm_config.h"
#include <Arduino.h>
#include <driver/i2s_std.h>
#include <driver/gpio.h>
#include <esp_log.h>

static const char *TAG = "audio";

#define CHUNK 160                       // frames estéreo por chamada ao driver

static i2s_chan_handle_t s_tx = nullptr, s_rx = nullptr;
static bool     s_mic = false;
static uint8_t  s_vol = COMM_VOLUME_DEFAULT;
static int32_t  s_frame[CHUNK * 2];     // RX: L,R intercalados
static int32_t  s_txf[CHUNK * 2];       // TX (outro buffer: leitura e escrita podem vir de tasks diferentes)

bool audio_init(void) {
    i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ch.dma_desc_num  = 6;
    ch.dma_frame_num = 240;             // 6 × 15 ms de folga no DMA
    ch.auto_clear    = true;            // sem dado novo o TX manda silêncio
    if (i2s_new_channel(&ch, &s_tx, &s_rx) != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel falhou");
        return false;
    }
    // Mesma config nos dois sentidos (full-duplex no mesmo controlador). Philips 32 bit:
    // BCLK = 64·fs, o que o INMP441 exige; MAX98357A e PCM5102A aceitam.
    i2s_std_config_t cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = (gpio_num_t)COMM_PIN_I2S_BCLK,
            .ws   = (gpio_num_t)COMM_PIN_I2S_WS,
            .dout = (gpio_num_t)COMM_PIN_I2S_DOUT,
            .din  = (gpio_num_t)COMM_PIN_I2S_DIN,
            .invert_flags = { false, false, false },
        },
    };
    if (i2s_channel_init_std_mode(s_tx, &cfg) != ESP_OK ||
        i2s_channel_init_std_mode(s_rx, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "i2s init falhou");
        return false;
    }
    // Sem mic o SD flutua; com pull-down lê zeros → detectável.
    gpio_pulldown_en((gpio_num_t)COMM_PIN_I2S_DIN);
    i2s_channel_enable(s_tx);
    i2s_channel_enable(s_rx);

    // Detecção do mic: ~100 ms depois do arranque (o INMP441 leva ~85 ms a acordar).
    static int32_t probe[1024];
    audio_read_mic(probe, 1024);
    audio_read_mic(probe, 1024);
    int32_t vmin = INT32_MAX, vmax = INT32_MIN;
    for (int i = 0; i < 1024; i++) {
        if (probe[i] < vmin) vmin = probe[i];
        if (probe[i] > vmax) vmax = probe[i];
    }
    s_mic = (vmax - vmin) > 16;
    // Diagnóstico: dado no slot direito = L/R do INMP441 solto ou em 3V3.
    size_t bytes = 0;
    int nz_l = 0, nz_r = 0;
    i2s_channel_read(s_rx, s_frame, sizeof(s_frame), &bytes, pdMS_TO_TICKS(100));
    for (size_t i = 0; i < bytes / (2 * sizeof(int32_t)); i++) {
        if (s_frame[2 * i])     nz_l++;
        if (s_frame[2 * i + 1]) nz_r++;
    }
    ESP_LOGI(TAG, "mic diag: min=%ld max=%ld nz_L=%d nz_R=%d (de %u) din_nivel=%d",
             (long)vmin, (long)vmax, nz_l, nz_r, (unsigned)(bytes / 8),
             gpio_get_level((gpio_num_t)COMM_PIN_I2S_DIN));
    ESP_LOGI(TAG, "I2S %d Hz bclk=%d ws=%d dout=%d din=%d — mic %s (span=%ld)",
             AUDIO_RATE_HZ, COMM_PIN_I2S_BCLK, COMM_PIN_I2S_WS, COMM_PIN_I2S_DOUT,
             COMM_PIN_I2S_DIN, s_mic ? "OK" : "ausente", (long)(vmax - vmin));
    return true;
}

bool audio_mic_present(void) { return s_mic; }

size_t audio_read_mic(int32_t *out, size_t n) {
    size_t got = 0;
    while (got < n) {
        size_t want = n - got > CHUNK ? CHUNK : n - got;
        size_t bytes = 0;
        if (i2s_channel_read(s_rx, s_frame, want * 2 * sizeof(int32_t), &bytes,
                             pdMS_TO_TICKS(100)) != ESP_OK && bytes == 0) break;
        size_t k = bytes / (2 * sizeof(int32_t));
        for (size_t i = 0; i < k; i++) out[got + i] = s_frame[2 * i] >> 8;   // L, 24 bit
        got += k;
    }
    return got;
}

void audio_flush_mic(void) {
    size_t bytes;
    // DMA RX tem ~6×240 frames; ler um pouco mais garante que o próximo bloco é novo.
    for (int i = 0; i < 10; i++)
        i2s_channel_read(s_rx, s_frame, sizeof(s_frame), &bytes, 0);
}

void audio_write(const int16_t *in, size_t n) {
    // Volume em Q8 sobre int16 → int32 alinhado à esquerda, mesmo sinal em L e R.
    int32_t g = (int32_t)s_vol * 256 / 100;
    size_t done = 0;
    while (done < n) {
        size_t k = n - done > CHUNK ? CHUNK : n - done;
        for (size_t i = 0; i < k; i++) {
            int32_t v = ((int32_t)in[done + i] * g) << 8;   // ×g/256, depois <<16
            s_txf[2 * i] = s_txf[2 * i + 1] = v;
        }
        size_t w;
        i2s_channel_write(s_tx, s_txf, k * 2 * sizeof(int32_t), &w, portMAX_DELAY);
        done += k;
    }
}

void audio_set_volume(uint8_t pct) { s_vol = pct > 100 ? 100 : pct; }
uint8_t audio_volume(void) { return s_vol; }
