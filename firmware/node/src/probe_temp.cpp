#include "probe_temp.h"
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_log.h>

static const char *TAG = "probe";
static const char *NAMES[PROBE_COUNT] = {"EXT", "SUP"};

typedef struct {
    int8_t            pin;
    OneWire           ow;
    DallasTemperature dt;
    DeviceAddress     addr;
    bool              present;
    bool              pending;   // conversão pedida, aguarda leitura no próximo tick
    uint8_t           misses;    // -127 consecutivos (CRC falhado ou sonda removida)
    bool              has_val;   // last.temp_c contém uma leitura real
    ProbeReading      last;
} probe_port_t;

static probe_port_t s_port[PROBE_COUNT];

static void port_detect(probe_port_t *p, ProbeId id) {
    p->dt.begin();
    if (p->dt.getDeviceCount() == 0 || !p->dt.getAddress(p->addr, 0)) {
        p->present = false;
        return;
    }
    p->dt.setResolution(p->addr, 12);
    p->dt.setWaitForConversion(false);   // não bloquear 750 ms
    p->present = true;
    ESP_LOGI(TAG, "sonda %s detectada em GPIO%d (%02X%02X…%02X)", NAMES[id], p->pin,
             p->addr[0], p->addr[1], p->addr[7]);
}

void probe_init(int8_t pin_ext, int8_t pin_sup) {
    int8_t pins[PROBE_COUNT] = {pin_ext, pin_sup};
    for (int i = 0; i < PROBE_COUNT; i++) {
        probe_port_t *p = &s_port[i];
        p->pin = pins[i];
        p->present = p->pending = false;
        p->last = {};
        if (p->pin < 0) continue;
        p->ow.begin(p->pin);
        p->dt.setOneWire(&p->ow);
        port_detect(p, (ProbeId)i);
        if (p->present) { p->dt.requestTemperatures(); p->pending = true; }
        else ESP_LOGI(TAG, "porta %s (GPIO%d) sem sonda", NAMES[i], p->pin);
    }
}

void probe_tick(void) {
    for (int i = 0; i < PROBE_COUNT; i++) {
        probe_port_t *p = &s_port[i];
        if (p->pin < 0) continue;
        p->last.valid = false;

        if (!p->present) {                      // hot-plug: procurar de novo
            port_detect(p, (ProbeId)i);
            if (p->present) { p->dt.requestTemperatures(); p->pending = true; }
            p->last.present = p->present;
            continue;
        }

        if (p->pending) {
            float t = p->dt.getTempC(p->addr);
            if (t == DEVICE_DISCONNECTED_C) {
                // -127 também sai num CRC falhado (ruído no cabo): só dar a sonda
                // como removida após 3 falhas seguidas, para não "piscar" no HA.
                if (++p->misses >= 3) {
                    ESP_LOGW(TAG, "sonda %s desligada", NAMES[i]);
                    p->present = p->pending = p->has_val = false;
                    p->misses = 0;
                    p->last.present = false;
                    continue;
                }
                ESP_LOGW(TAG, "sonda %s: leitura falhou (%u/3) — mantém último valor", NAMES[i], p->misses);
                p->last.valid = p->has_val;          // segura o último valor (≤ 16 s)
            } else {
                p->misses = 0;
                // 85.0 °C = valor de power-on do DS18B20 (conversão não aconteceu)
                if (t != 85.0f && t > -55.0f && t < 125.0f) {
                    p->last.temp_c = t;
                    p->last.valid  = true;
                    p->has_val     = true;
                }
            }
        }
        p->dt.requestTemperatures();
        p->pending = true;
        p->last.present = true;
    }
}

ProbeReading probe_get(ProbeId id) {
    if (id >= PROBE_COUNT) return ProbeReading{};
    return s_port[id].last;
}
