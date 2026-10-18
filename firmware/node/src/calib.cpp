#include "calib.h"
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include <math.h>
#include <esp_log.h>

static const char *TAG = "calib";
static Calib s_cal = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f};

typedef struct { const char *k; float *v; float lo, hi, def; } item_t;
static item_t s_items[] = {
    {"t",   &s_cal.t_aht,  -10.0f, 10.0f, 0.0f},
    {"rh",  &s_cal.rh_aht, -20.0f, 20.0f, 0.0f},
    {"db",  &s_cal.db,     -30.0f, 30.0f, 0.0f},
    {"ct",  &s_cal.ct,       0.2f,  5.0f, 1.0f},
    {"lux", &s_cal.lux,      0.1f, 10.0f, 1.0f},
};
#define N_ITEMS (sizeof(s_items) / sizeof(s_items[0]))

void calib_load(void) {
    Preferences p;
    if (!p.begin("calib", false)) return;
    for (size_t i = 0; i < N_ITEMS; i++) {
        float v = p.isKey(s_items[i].k) ? p.getFloat(s_items[i].k, s_items[i].def) : s_items[i].def;
        if (!isfinite(v) || v < s_items[i].lo || v > s_items[i].hi) v = s_items[i].def;
        *s_items[i].v = v;
    }
    p.end();
    ESP_LOGI(TAG, "T%+.2f UR%+.1f dB%+.1f ct×%.3f lux×%.3f",
             s_cal.t_aht, s_cal.rh_aht, s_cal.db, s_cal.ct, s_cal.lux);
}

const Calib &calib(void) { return s_cal; }

bool calib_set(const char *k, float v) {
    for (size_t i = 0; i < N_ITEMS; i++) {
        if (strcmp(k, s_items[i].k) != 0) continue;
        if (!isfinite(v) || v < s_items[i].lo || v > s_items[i].hi) return false;
        *s_items[i].v = v;
        Preferences p;
        if (p.begin("calib", false)) { p.putFloat(s_items[i].k, v); p.end(); }
        ESP_LOGI(TAG, "%s = %.3f (gravado)", k, v);
        return true;
    }
    return false;
}

void calib_reset(void) {
    Preferences p;
    if (p.begin("calib", false)) { p.clear(); p.end(); }
    for (size_t i = 0; i < N_ITEMS; i++) *s_items[i].v = s_items[i].def;
    ESP_LOGI(TAG, "calibração zerada");
}
