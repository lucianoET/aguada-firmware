#include "body.h"
#include "yeti_config.h"
#include <Arduino.h>
#include <esp_log.h>
#include <math.h>

static const char *TAG = "body";

// ── Braços: touchRead sobe quando tocado (S3). Linha de base adaptativa ─────────
struct Arm {
    uint8_t  pin;
    float    base = 0;
    bool     down = false;
    uint8_t  hits = 0;
    uint32_t t0 = 0;
    bool     held = false;
};
static Arm s_arm[2] = { { YETI_PIN_ARM_L }, { YETI_PIN_ARM_R } };

static void arm_calibrate(Arm &a) {
    uint32_t acc = 0;
    for (int i = 0; i < 16; i++) { acc += touchRead(a.pin); delay(2); }
    a.base = acc / 16.0f;
}

// true enquanto tocado (histerese + 2 leituras seguidas)
static bool arm_sample(Arm &a) {
    float v = touchRead(a.pin);
    float r = a.base > 0 ? v / a.base : 1;
    if (!a.down) {
        a.hits = r > YETI_ARM_ON ? a.hits + 1 : 0;
        if (a.hits >= 2) { a.down = true; a.hits = 0; }
        else a.base += (v - a.base) * 0.002f;          // segue deriva lenta (umidade, temp.)
    } else if (r < YETI_ARM_OFF) {
        a.down = false;
    }
    return a.down;
}

ArmEvt body_arms_tick(void) {
    static uint32_t last = 0, both_t = 0;
    static bool hug = false;
    uint32_t now = millis();
    if (now - last < 5) return ARM_NONE;
    last = now;

    bool d[2];
    ArmEvt ev = ARM_NONE;
    for (int i = 0; i < 2; i++) {
        Arm &a = s_arm[i];
        bool was = a.down;
        d[i] = arm_sample(a);
        if (d[i] && !was) { a.t0 = now; a.held = false; }
        if (d[i] && !a.held && !hug && now - a.t0 >= YETI_ARM_HOLD_MS) {
            a.held = true;
            ev = i ? ARM_HOLD_R : ARM_HOLD_L;
        }
        if (!d[i] && was && !a.held && !hug && now - a.t0 < YETI_ARM_HOLD_MS) {
            // espera um pouco: o outro braço pode estar chegando (abraço)
            if (!s_arm[1 - i].down) ev = i ? ARM_TAP_R : ARM_TAP_L;
        }
    }
    if (d[0] && d[1]) {
        if (!both_t) both_t = now;
        if (!hug && now - both_t > 150) { hug = true; ev = ARM_HUG; }
    } else {
        both_t = 0;
        if (!d[0] && !d[1]) hug = false;
    }
    return ev;
}

// ── WS2812 por RMT (10 MHz: 0 = 400/800 ns, 1 = 800/400 ns), ordem GRB ─────────
static bool     s_led_ok = false;
static uint8_t  s_flash[3];
static uint32_t s_flash_until = 0;

static void leds_write(uint8_t r, uint8_t g, uint8_t b) {
    if (!s_led_ok) return;
    static rmt_data_t sym[24 * YETI_LED_COUNT];
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    int k = 0;
    for (int led = 0; led < YETI_LED_COUNT; led++)
        for (int bit = 23; bit >= 0; bit--) {
            bool one = grb & (1UL << bit);
            sym[k].level0 = 1; sym[k].duration0 = one ? 8 : 4;
            sym[k].level1 = 0; sym[k].duration1 = one ? 4 : 8;
            k++;
        }
    rmtWrite(YETI_PIN_LED, sym, k, RMT_WAIT_FOR_EVER);
}

void body_leds_flash(uint8_t r, uint8_t g, uint8_t b, uint16_t ms) {
    s_flash[0] = r; s_flash[1] = g; s_flash[2] = b;
    s_flash_until = millis() + ms;
}

void body_leds(Mood m, uint8_t bright, float level) {
    static uint32_t last = 0;
    static uint8_t cur[3] = { 255, 255, 255 };
    uint32_t now = millis();
    if (now - last < 40) return;
    last = now;

    float r = 0, g = 0, b = 0, k = 1;
    switch (m) {
        case MOOD_ALARM:  r = 1; k = (now / 250) % 2; break;
        case MOOD_TALK:   g = 1; k = 0.3f + 0.7f * level; break;
        case MOOD_LISTEN: b = 1; g = 0.4f; k = 0.3f + 0.7f * level; break;
        case MOOD_HAPPY:  r = 1; g = 0.25f; b = 0.45f; break;          // rosado
        case MOOD_COLD:   b = 1; g = 0.3f; break;
        case MOOD_HOT:    r = 1; g = 0.35f; break;
        case MOOD_DIZZY:  r = 0.6f; b = 1; k = 0.5f + 0.5f * sinf(now / 120.0f); break;
        case MOOD_SLEEPY: k = 0; break;
        default:          r = 1; g = 0.35f; b = 0.4f; k = 0.35f; break;   // bochecha calma
    }
    float s = k * bright / 255.0f * YETI_LED_MAX / 255.0f;
    uint8_t c[3] = { (uint8_t)(r * s * 255), (uint8_t)(g * s * 255), (uint8_t)(b * s * 255) };
    if (now < s_flash_until) for (int i = 0; i < 3; i++) c[i] = s_flash[i];
    if (!memcmp(c, cur, 3)) return;
    memcpy(cur, c, 3);
    leds_write(c[0], c[1], c[2]);
}

// ── Bateria: divisor no ADC1, média móvel ───────────────────────────────────────
static float s_vbat = NAN;

static void vbat_sample(void) {
    float v = analogReadMilliVolts(YETI_PIN_VBAT) * YETI_VBAT_DIV / 1000.0f;
    if (v < 2.5f) { s_vbat = NAN; return; }      // sem divisor (pino em ~0) ou bateria morta
    s_vbat = isnan(s_vbat) ? v : s_vbat + (v - s_vbat) * 0.1f;
}

float body_vbat(void) {
    static uint32_t last = 0;
    if (millis() - last > 1000) { last = millis(); vbat_sample(); }
    return s_vbat;
}

int body_vbat_pct(void) {
    float v = body_vbat();
    if (isnan(v)) return -1;
    // LiPo 1S em repouso, aproximação por trechos
    static const float tv[] = { 3.30f, 3.60f, 3.70f, 3.80f, 3.95f, 4.10f, 4.20f };
    static const float tp[] = { 0,     10,    25,    45,    70,    90,    100 };
    if (v <= tv[0]) return 0;
    for (int i = 1; i < 7; i++)
        if (v <= tv[i]) return (int)(tp[i - 1] + (v - tv[i - 1]) / (tv[i] - tv[i - 1]) * (tp[i] - tp[i - 1]));
    return 100;
}

void body_begin(void) {
    for (Arm &a : s_arm) arm_calibrate(a);
    ESP_LOGI(TAG, "braços: base E=%.0f D=%.0f (toque > x%.2f)", s_arm[0].base, s_arm[1].base,
             (double)YETI_ARM_ON);
    s_led_ok = rmtInit(YETI_PIN_LED, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000);
    leds_write(0, 0, 0);
    analogSetPinAttenuation(YETI_PIN_VBAT, ADC_11db);
    vbat_sample();
    ESP_LOGI(TAG, "leds %s, bateria %.2f V", s_led_ok ? "ok" : "FALHOU", (double)s_vbat);
}
