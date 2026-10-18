#include "airq_test.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>

static const char *TAG = "test";
static const char *COLS = "[\"ta\",\"rh\",\"tr\",\"ts\",\"te\",\"a\",\"db\",\"co2\",\"eco2\",\"tvoc\",\"lux\",\"aqi\"]";

static TestSample s_buf[TEST_MAXN];
static uint16_t   s_n = 0;
static TestKind   s_kind = TEST_NONE;
static TestState  s_state = TS_IDLE;
static uint32_t   s_t0 = 0, s_dur = 0, s_tend = 0;
static char       s_tag[65] = "", s_opt[97] = "";

// Copia só caracteres seguros para JSON (sem aspas, barras ou controlo).
static void safe_copy(char *dst, size_t n, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 1 < n; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x20 || c == '"' || c == '\\') continue;
        dst[j++] = (char)c;
    }
    dst[j] = 0;
}

void test_start(TestKind kind, uint32_t dur_s, const char *tag, const char *opt) {
    uint32_t max_s = (uint32_t)TEST_MAXN * 8;
    if (dur_s < 60) dur_s = 60;
    if (dur_s > max_s) dur_s = max_s;
    s_kind = kind; s_state = TS_RUN; s_n = 0;
    s_t0 = millis(); s_dur = dur_s; s_tend = 0;
    safe_copy(s_tag, sizeof(s_tag), tag);
    safe_copy(s_opt, sizeof(s_opt), opt);
    ESP_LOGI(TAG, "teste %s iniciado (%lu s) '%s'", kind == TEST_AC ? "AC" : "ambiente",
             (unsigned long)dur_s, s_tag);
}

void test_stop(void) {
    if (s_state != TS_RUN) return;
    s_state = TS_DONE;
    s_tend = millis();
    ESP_LOGI(TAG, "teste terminado (%u amostras)", s_n);
}

void test_add(const TestSample &s) {
    if (s_state != TS_RUN) return;
    if (s_n < TEST_MAXN) s_buf[s_n++] = s;
    if (millis() - s_t0 >= s_dur * 1000UL || s_n >= TEST_MAXN) test_stop();
}

TestState test_state(void) { return s_state; }
TestKind  test_kind(void)  { return s_kind; }

void test_json(test_emit_fn emit, void *ctx) {
    char b[256];
    uint32_t el = s_state == TS_IDLE ? 0 : ((s_state == TS_RUN ? millis() : s_tend) - s_t0) / 1000;
    snprintf(b, sizeof(b),
             "{\"kind\":\"%s\",\"state\":\"%s\",\"el\":%lu,\"dur\":%lu,\"step\":8,\"n\":%u,"
             "\"tag\":\"%s\",\"opt\":\"%s\",\"cols\":%s,\"d\":[",
             s_kind == TEST_AC ? "ac" : s_kind == TEST_ENV ? "env" : "",
             s_state == TS_RUN ? "run" : s_state == TS_DONE ? "done" : "idle",
             (unsigned long)el, (unsigned long)s_dur, s_n, s_tag, s_opt, COLS);
    emit(b, ctx);
    // ~16 amostras por pedaço
    size_t len = 0;
    b[0] = 0;
    for (uint16_t i = 0; i < s_n; i++) {
        len += snprintf(b + len, sizeof(b) - len, "%s[", i ? "," : "");
        for (int c = 0; c < TEST_NCOL; c++) {
            int16_t v = s_buf[i].v[c];
            if (v == TEST_NULL) len += snprintf(b + len, sizeof(b) - len, "%snull", c ? "," : "");
            else                len += snprintf(b + len, sizeof(b) - len, "%s%d", c ? "," : "", v);
        }
        len += snprintf(b + len, sizeof(b) - len, "]");
        if (len > sizeof(b) - 110) { emit(b, ctx); len = 0; b[0] = 0; }
    }
    if (len) emit(b, ctx);
    emit("]}", ctx);
}
