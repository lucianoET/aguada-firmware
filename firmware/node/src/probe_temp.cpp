#include "probe_temp.h"
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <string.h>
#include <esp_log.h>

static const char *TAG = "probe";
static const char *NAMES[PROBE_COUNT] = {"EXT", "SUP", "RET"};
static const char *KEYS[PROBE_COUNT]  = {"ext", "sup", "ret"};
#define MAX_DEV 4   // além disso o barramento está errado; extras são ignoradas

typedef struct {
    DeviceAddress addr;      // ROM amarrada a este papel (vale se has_addr)
    bool          has_addr;
    bool          present;   // sonda deste papel está no barramento
    uint8_t       misses;    // -127 consecutivos (CRC falhado ou sonda removida)
    bool          has_val;   // last.temp_c contém uma leitura real
    ProbeReading  last;
} slot_t;

static int8_t            s_pin = -1;
static OneWire           s_ow;
static DallasTemperature s_dt;
static slot_t            s_slot[PROBE_COUNT];
static bool              s_pending = false;   // conversão pedida, ler no próximo tick

// Todas as sondas vistas na última varredura (com ou sem papel) — base do identificador
typedef struct {
    DeviceAddress addr;
    float         t;          // última leitura (modo identificador)
    float         base;       // referência = 1ª leitura após iniciar o identificador
    bool          ok;         // t válida
    bool          has_base;
} busdev_t;
static busdev_t s_bus[MAX_DEV];
static uint8_t  s_nbus = 0;

#define IDENT_MS        300000UL   // modo identificador dura 5 min
#define IDENT_PERIOD_MS 1000UL     // 1 leitura/s (conversão 12 bit = 750 ms)
#define IDENT_SCAN_MS   5000UL     // re-varre o barramento (sonda ligada a meio)
static uint32_t s_ident_until = 0, s_ident_last = 0, s_ident_scan = 0;
static bool     s_ident_on = false, s_ident_pending = false;

// Guarda a lista da varredura preservando leitura/referência das ROMs já conhecidas.
static void bus_list_update(DeviceAddress *found, uint8_t nf) {
    busdev_t nb[MAX_DEV] = {};
    for (uint8_t f = 0; f < nf; f++) {
        memcpy(nb[f].addr, found[f], 8);
        for (uint8_t o = 0; o < s_nbus; o++) {
            if (memcmp(s_bus[o].addr, found[f], 8) == 0) {
                nb[f].t = s_bus[o].t; nb[f].base = s_bus[o].base;
                nb[f].ok = s_bus[o].ok; nb[f].has_base = s_bus[o].has_base;
                break;
            }
        }
    }
    memcpy(s_bus, nb, sizeof(nb));
    s_nbus = nf;
}

static void rom_hex(const uint8_t *a, char *buf) {
    for (int i = 0; i < 8; i++) snprintf(buf + 2 * i, 3, "%02X", a[i]);
    buf[16] = 0;
}

static void nvs_save(void) {
    Preferences p;
    if (!p.begin("probes", false)) { ESP_LOGW(TAG, "NVS indisponível — papéis não gravados"); return; }
    for (int i = 0; i < PROBE_COUNT; i++) {
        if (s_slot[i].has_addr) p.putBytes(KEYS[i], s_slot[i].addr, 8);
        else if (p.isKey(KEYS[i])) p.remove(KEYS[i]);
    }
    p.end();
}

static void nvs_load(void) {
    Preferences p;
    if (!p.begin("probes", false)) return;
    for (int i = 0; i < PROBE_COUNT; i++) {
        slot_t *s = &s_slot[i];
        s->has_addr = p.isKey(KEYS[i]) && p.getBytes(KEYS[i], s->addr, 8) == 8 &&
                      OneWire::crc8(s->addr, 7) == s->addr[7];
    }
    p.end();
}

// Varre o barramento e casa as ROMs encontradas com os papéis EXT/SUP.
// Papel ainda presente (a cair só por falhas de leitura) não é "livre": um sumiço
// momentâneo na busca não deixa outra sonda roubar o lugar.
static void bus_scan(void) {
    s_dt.begin();
    s_dt.setWaitForConversion(false);   // não bloquear 750 ms
    uint8_t n = s_dt.getDeviceCount();
    DeviceAddress found[MAX_DEV];
    uint8_t nf = 0;
    for (uint8_t i = 0; i < n && nf < MAX_DEV; i++)
        if (s_dt.getAddress(found[nf], i) && s_dt.validFamily(found[nf])) nf++;
    bus_list_update(found, nf);
    if (n > PROBE_COUNT) ESP_LOGW(TAG, "%u sondas no barramento — só %d papéis (EXT/SUP/RET)", n, PROBE_COUNT);

    bool used[MAX_DEV] = {};
    bool seen[PROBE_COUNT] = {};
    for (int s = 0; s < PROBE_COUNT; s++) {
        if (!s_slot[s].has_addr) continue;
        for (uint8_t f = 0; f < nf; f++)
            if (!used[f] && memcmp(found[f], s_slot[s].addr, 8) == 0) { seen[s] = used[f] = true; break; }
    }

    bool changed = false;
    char rom[17];
    for (uint8_t f = 0; f < nf; f++) {
        if (used[f]) continue;
        for (int s = 0; s < PROBE_COUNT; s++) {
            if (seen[s] || s_slot[s].present) continue;   // papel ocupado
            memcpy(s_slot[s].addr, found[f], 8);
            s_slot[s].has_addr = true;
            s_slot[s].has_val  = false;
            seen[s] = used[f] = changed = true;
            rom_hex(found[f], rom);
            ESP_LOGI(TAG, "sonda nova %s → papel %s (gravado)", rom, NAMES[s]);
            break;
        }
        if (!used[f]) { rom_hex(found[f], rom); ESP_LOGW(TAG, "sonda %s sem papel livre — ignorada", rom); }
    }

    for (int s = 0; s < PROBE_COUNT; s++) {
        slot_t *p = &s_slot[s];
        if (seen[s] && !p->present) {
            p->present = true;
            p->misses  = 0;
            rom_hex(p->addr, rom);
            ESP_LOGI(TAG, "sonda %s detectada (GPIO%d, ROM %s)", NAMES[s], s_pin, rom);
        }
    }
    if (nf) s_dt.setResolution(12);   // todas as sondas do barramento
    if (changed) nvs_save();
}

void probe_init(int8_t pin) {
    s_pin = pin;
    s_pending = false;
    memset(s_slot, 0, sizeof(s_slot));
    if (pin < 0) return;
    nvs_load();
    s_ow.begin(pin);
    s_dt.setOneWire(&s_ow);
    bus_scan();
    char rom[17];
    for (int s = 0; s < PROBE_COUNT; s++) {
        if (s_slot[s].present) continue;
        if (s_slot[s].has_addr) {
            rom_hex(s_slot[s].addr, rom);
            ESP_LOGI(TAG, "papel %s (ROM %s) sem sonda no barramento GPIO%d", NAMES[s], rom, pin);
        } else {
            ESP_LOGI(TAG, "papel %s livre — sem sonda no barramento GPIO%d", NAMES[s], pin);
        }
    }
    for (int s = 0; s < PROBE_COUNT; s++) s_pending |= s_slot[s].present;
    if (s_pending) s_dt.requestTemperatures();
}

void probe_tick(void) {
    if (s_pin < 0) return;

    // 1. Ler a conversão pedida no tick anterior
    for (int i = 0; i < PROBE_COUNT; i++) {
        slot_t *p = &s_slot[i];
        p->last.valid = false;
        if (!p->present || !s_pending) continue;
        float t = s_dt.getTempC(p->addr);
        if (t == DEVICE_DISCONNECTED_C) {
            // -127 também sai num CRC falhado (ruído no cabo): só dar a sonda
            // como removida após 3 falhas seguidas, para não "piscar" no HA.
            if (++p->misses >= 3) {
                ESP_LOGW(TAG, "sonda %s desligada", NAMES[i]);
                p->present = p->has_val = false;
                p->misses = 0;
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

    // 2. Hot-plug: falta alguma sonda → varrer de novo (depois de ler, antes de pedir)
    bool missing = false, any = false;
    for (int i = 0; i < PROBE_COUNT; i++) missing |= !s_slot[i].present;
    if (missing) bus_scan();

    // 3. Pedir a próxima conversão (comando único para o barramento todo)
    for (int i = 0; i < PROBE_COUNT; i++) any |= s_slot[i].present;
    s_pending = any;
    if (s_pending) s_dt.requestTemperatures();
    for (int i = 0; i < PROBE_COUNT; i++) s_slot[i].last.present = s_slot[i].present;
}

ProbeReading probe_get(ProbeId id) {
    if (id >= PROBE_COUNT) return ProbeReading{};
    return s_slot[id].last;
}

void probe_swap(ProbeId a, ProbeId b) {
    if (a >= PROBE_COUNT || b >= PROBE_COUNT || a == b) return;
    slot_t t = s_slot[a];
    s_slot[a] = s_slot[b];
    s_slot[b] = t;
    nvs_save();
    ESP_LOGI(TAG, "papéis trocados: %s↔%s (gravado)", NAMES[a], NAMES[b]);
}

const char *probe_name(ProbeId id) { return id < PROBE_COUNT ? NAMES[id] : "?"; }

void probe_rom_str(ProbeId id, char *buf) {
    buf[0] = 0;
    if (id < PROBE_COUNT && s_slot[id].has_addr) rom_hex(s_slot[id].addr, buf);
}

uint8_t probe_bus_count(void) { return s_nbus; }

// ── Identificador de sondas ──────────────────────────────────────────────────

void probe_ident_start(void) {
    if (s_pin < 0) return;
    s_ident_on      = true;
    s_ident_until   = millis() + IDENT_MS;
    s_ident_last    = 0;
    s_ident_scan    = 0;
    s_ident_pending = false;
    for (uint8_t b = 0; b < MAX_DEV; b++) { s_bus[b].has_base = false; s_bus[b].ok = false; }
    ESP_LOGI(TAG, "identificador ligado (5 min) — aqueça uma sonda de cada vez");
}

void probe_ident_stop(void) {
    if (s_ident_on) ESP_LOGI(TAG, "identificador desligado");
    s_ident_on = false;
}

bool probe_ident_active(void) {
    if (s_ident_on && (int32_t)(millis() - s_ident_until) >= 0) {
        s_ident_on = false;
        ESP_LOGI(TAG, "identificador expirou");
    }
    return s_ident_on;
}

void probe_ident_tick(void) {
    if (!probe_ident_active()) return;
    uint32_t now = millis();
    if (s_ident_last && now - s_ident_last < IDENT_PERIOD_MS) return;
    s_ident_last = now;

    // 1. ler a conversão pedida há ~1 s (falha isolada mantém o último valor)
    if (s_ident_pending) {
        for (uint8_t b = 0; b < s_nbus; b++) {
            float t = s_dt.getTempC(s_bus[b].addr);
            if (t == DEVICE_DISCONNECTED_C || t == 85.0f || t <= -55.0f || t >= 125.0f) continue;
            s_bus[b].t  = t;
            s_bus[b].ok = true;
            if (!s_bus[b].has_base) { s_bus[b].base = t; s_bus[b].has_base = true; }
        }
    }
    // 2. sonda ligada a meio do processo
    if (!s_ident_scan || now - s_ident_scan >= IDENT_SCAN_MS) { bus_scan(); s_ident_scan = now; }
    // 3. próxima conversão (todas as sondas de uma vez)
    s_ident_pending = s_nbus > 0;
    if (s_ident_pending) s_dt.requestTemperatures();
}

static bool parse_rom(const char *h, uint8_t *a) {
    if (!h || strlen(h) != 16) return false;
    for (int i = 0; i < 8; i++) {
        char b[3] = {h[2 * i], h[2 * i + 1], 0};
        char *e = nullptr;
        long v = strtol(b, &e, 16);
        if (!e || *e) return false;
        a[i] = (uint8_t)v;
    }
    return OneWire::crc8(a, 7) == a[7];
}

bool probe_assign(const char *rom_hex16, ProbeId role) {
    if (role >= PROBE_COUNT) return false;
    uint8_t a[8];
    if (!parse_rom(rom_hex16, a)) return false;

    int cur = -1;
    for (int s = 0; s < PROBE_COUNT; s++)
        if (s_slot[s].has_addr && memcmp(s_slot[s].addr, a, 8) == 0) cur = s;
    if (cur == role) return true;
    if (cur >= 0) { probe_swap((ProbeId)cur, role); return true; }   // já tinha papel → troca

    bool on_bus = false;
    for (uint8_t b = 0; b < s_nbus; b++) on_bus |= memcmp(s_bus[b].addr, a, 8) == 0;
    if (!on_bus) return false;

    // sonda sem papel assume `role`; a ROM anterior desse papel fica sem papel
    slot_t *p = &s_slot[role];
    memset(p, 0, sizeof(*p));
    memcpy(p->addr, a, 8);
    p->has_addr = true;
    p->present  = true;
    nvs_save();
    ESP_LOGI(TAG, "sonda %s → papel %s (gravado)", rom_hex16, NAMES[role]);
    return true;
}

void probe_ident_json(char *buf, size_t len) {
    bool on = probe_ident_active();
    unsigned long left = on ? (unsigned long)((s_ident_until - millis()) / 1000) : 0;
    size_t n = (size_t)snprintf(buf, len, "{\"ident\":%s,\"left\":%lu,\"dev\":[", on ? "true" : "false", left);
    char rom[17];
    for (uint8_t b = 0; b < s_nbus && n < len; b++) {
        const char *role = "";
        for (int s = 0; s < PROBE_COUNT; s++)
            if (s_slot[s].has_addr && memcmp(s_slot[s].addr, s_bus[b].addr, 8) == 0) role = NAMES[s];
        rom_hex(s_bus[b].addr, rom);
        const busdev_t &d = s_bus[b];
        n += (size_t)snprintf(buf + n, len - n,
                              "%s{\"rom\":\"%s\",\"role\":\"%s\",\"t\":%.2f,\"d\":%.2f,\"ok\":%s}",
                              b ? "," : "", rom, role, d.t,
                              (d.ok && d.has_base) ? d.t - d.base : 0.0f, d.ok ? "true" : "false");
    }
    if (n < len) snprintf(buf + n, len - n, "]}");
}
