#include "co2_scd41.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_log.h>

static const char *TAG = "scd41";

#define SCD41_ADDR              0x62
#define CMD_START_PERIODIC      0x21B1
#define CMD_STOP_PERIODIC       0x3F86
#define CMD_GET_DATA_READY      0xE4B8
#define CMD_READ_MEASUREMENT    0xEC05
#define CMD_GET_SERIAL          0x3682
// Leitura considerada "fresca" até 3 períodos de medição (5 s cada).
#define SCD41_STALE_MS          20000UL

static bool       s_present = false;
static Co2Reading s_last    = {};
static uint32_t   s_last_ms = 0;

static uint8_t crc8(const uint8_t *d, int n) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < n; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
    return crc;
}

static bool send_cmd(uint16_t cmd) {
    Wire.beginTransmission(SCD41_ADDR);
    Wire.write((uint8_t)(cmd >> 8));
    Wire.write((uint8_t)(cmd & 0xFF));
    return Wire.endTransmission() == 0;
}

// Lê n palavras de 16 bit (cada uma seguida de CRC8). Falha se CRC não bater.
static bool read_words(uint16_t *out, int n) {
    int len = n * 3;
    if (Wire.requestFrom((uint8_t)SCD41_ADDR, (uint8_t)len) != len) return false;
    for (int i = 0; i < n; i++) {
        uint8_t b[3];
        b[0] = Wire.read(); b[1] = Wire.read(); b[2] = Wire.read();
        if (crc8(b, 2) != b[2]) return false;
        out[i] = ((uint16_t)b[0] << 8) | b[1];
    }
    return true;
}

bool scd41_init(void) {
    s_present = false;
    // Se o sensor já estava em medição periódica (reset só do ESP32), tem de parar
    // antes de aceitar outros comandos. Sem ACK aqui = sensor ausente.
    if (!send_cmd(CMD_STOP_PERIODIC)) {
        ESP_LOGI(TAG, "SCD41 não detectado (0x%02X) — CO2 só estimado (ENS160)", SCD41_ADDR);
        return false;
    }
    delay(500);

    uint16_t sn[3];
    if (!send_cmd(CMD_GET_SERIAL)) return false;
    delay(1);
    if (!read_words(sn, 3)) {
        ESP_LOGW(TAG, "SCD41 responde mas serial com CRC inválido");
        return false;
    }
    if (!send_cmd(CMD_START_PERIODIC)) return false;

    s_present = true;
    ESP_LOGI(TAG, "SCD41 OK serial=%04X%04X%04X — medição periódica 5 s", sn[0], sn[1], sn[2]);
    return true;
}

bool scd41_present(void) { return s_present; }

Co2Reading scd41_read(void) {
    Co2Reading r = {};
    if (!s_present) return r;

    uint16_t st = 0;
    bool ready = send_cmd(CMD_GET_DATA_READY) && (delay(1), read_words(&st, 1)) && (st & 0x07FF);
    if (ready) {
        uint16_t w[3];
        if (send_cmd(CMD_READ_MEASUREMENT) && (delay(1), read_words(w, 3)) && w[0] != 0) {
            s_last.co2_ppm = w[0];
            s_last.temp_c  = -45.0f + 175.0f * (float)w[1] / 65535.0f;
            s_last.hum_pct = 100.0f * (float)w[2] / 65535.0f;
            s_last.valid   = true;
            s_last_ms      = millis();
            ESP_LOGI(TAG, "CO2=%uppm (T_int=%.1fC RH=%.0f%%)", s_last.co2_ppm, s_last.temp_c, s_last.hum_pct);
        } else {
            ESP_LOGW(TAG, "leitura SCD41 falhou");
        }
    }
    if (s_last.valid && millis() - s_last_ms <= SCD41_STALE_MS) r = s_last;
    return r;
}
