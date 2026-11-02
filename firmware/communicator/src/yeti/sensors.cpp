#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <esp_log.h>
#include "yeti_config.h"
#include "sensors.h"

static const char *TAG = "sens";

static YetiSensors s;
static uint8_t s_imu_addr = 0, s_lux_addr = 0;

const YetiSensors &sensors(void) { return s; }

static bool probe(uint8_t a) {
    Wire.beginTransmission(a);
    return Wire.endTransmission() == 0;
}

static bool wr(uint8_t a, uint8_t reg, int val = -1) {
    Wire.beginTransmission(a);
    Wire.write(reg);
    if (val >= 0) Wire.write((uint8_t)val);
    return Wire.endTransmission() == 0;
}

static int rd(uint8_t a, uint8_t *buf, int n) {
    int got = Wire.requestFrom((int)a, n);
    for (int i = 0; i < got; i++) buf[i] = Wire.read();
    return got;
}

// ── HTU21D (0x40): medida sem "hold master", tempo máx. 50 ms (T) e 16 ms (RH) ──
#define HTU_ADDR 0x40

static bool htu_read(uint8_t cmd, uint32_t wait_ms, uint16_t *raw) {
    if (!wr(HTU_ADDR, cmd)) return false;
    vTaskDelay(pdMS_TO_TICKS(wait_ms));
    uint8_t b[3];
    if (rd(HTU_ADDR, b, 3) != 3) return false;
    *raw = ((uint16_t)b[0] << 8 | b[1]) & 0xFFFC;
    return true;
}

static void htu_update(void) {
    uint16_t r;
    if (htu_read(0xF3, 55, &r)) s.temp_c = -46.85f + 175.72f * r / 65536.0f;
    if (htu_read(0xF5, 20, &r)) s.rh = constrain(-6.0f + 125.0f * r / 65536.0f, 0.0f, 100.0f);
}

// ── MPU6050/6500/9250: ±2 g, ±250 °/s, DLPF 44 Hz ──────────────────────────────
static void imu_init(void) {
    wr(s_imu_addr, 0x6B, 0x01);   // acorda, clock do giro X
    wr(s_imu_addr, 0x1A, 0x03);   // DLPF
    wr(s_imu_addr, 0x1B, 0x00);
    wr(s_imu_addr, 0x1C, 0x00);
}

static void imu_update(void) {
    uint8_t b[14];
    if (!wr(s_imu_addr, 0x3B) || rd(s_imu_addr, b, 14) != 14) return;
    auto v = [&](int i) { return (int16_t)(b[i] << 8 | b[i + 1]); };
    s.ax = v(0) / 16384.0f; s.ay = v(2) / 16384.0f; s.az = v(4) / 16384.0f;
    s.gx = v(8) / 131.0f;   s.gy = v(10) / 131.0f;  s.gz = v(12) / 131.0f;

    // Chacoalhão: |a| longe de 1 g em vários quadros seguidos. Batida: pico isolado.
    static uint8_t hard = 0;
    float dev = fabsf(sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az) - 1.0f);
    if (dev > 0.8f) {
        if (++hard >= 6) { s.shake_ms = millis(); hard = 0; }
    } else {
        if (hard >= 1 && hard <= 2) s.tap_ms = millis();
        hard = 0;
    }
}

// ── BH1750: modo contínuo alta resolução (1 lx, 120 ms) ────────────────────────
static void lux_update(void) {
    uint8_t b[2];
    if (rd(s_lux_addr, b, 2) == 2) s.lux_v = ((uint16_t)b[0] << 8 | b[1]) / 1.2f;
}

static void sensors_task(void *) {
    uint32_t t_htu = 0, t_lux = 0, t_log = 0;
    for (;;) {
        uint32_t now = millis();
        if (s.imu) imu_update();
        if (s.lux && now - t_lux >= 200) { t_lux = now; lux_update(); }
        if (s.htu && (t_htu == 0 || now - t_htu >= 5000)) { t_htu = now; htu_update(); }
        if (now - t_log >= 10000) {
            t_log = now;
            Serial.printf("YETI t=%.1fC rh=%.0f%% lux=%.0f a=(%.2f %.2f %.2f)\n",
                          s.temp_c, s.rh, s.lux_v, s.ax, s.ay, s.az);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void sensors_begin(void) {
    Wire.begin(YETI_PIN_SDA, YETI_PIN_SCL, 400000);
    Serial.print("[i2c] scan:");
    for (uint8_t a = 1; a < 127; a++)
        if (probe(a)) Serial.printf(" 0x%02X", a);
    Serial.println();

    if (probe(HTU_ADDR)) {
        wr(HTU_ADDR, 0xFE);           // soft reset
        delay(15);
        s.htu = true;
    }
    for (uint8_t a : { 0x68, 0x69 }) {
        uint8_t who = 0;
        if (probe(a) && wr(a, 0x75) && rd(a, &who, 1) == 1) {
            s_imu_addr = a;
            s.imu = true;
            ESP_LOGI(TAG, "IMU 0x%02X WHO_AM_I=0x%02X", a, who);
            imu_init();
            break;
        }
    }
    for (uint8_t a : { 0x23, 0x5C }) {
        if (probe(a) && wr(a, 0x01) && wr(a, 0x10)) { s_lux_addr = a; s.lux = true; break; }
    }
    ESP_LOGI(TAG, "HTU21D %s, IMU %s, BH1750 %s", s.htu ? "ok" : "--", s.imu ? "ok" : "--",
             s.lux ? "ok" : "--");
    xTaskCreate(sensors_task, "sens", 3072, nullptr, 1, nullptr);
}
