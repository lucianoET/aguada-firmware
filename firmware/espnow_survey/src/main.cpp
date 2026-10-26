// Assistente de enlace ESP-NOW (Aguada): no ponto onde está, mede RSSI e perda de cada node
// e pinga node ou gateway. O ping é unicast de 1 byte: o rádio do destino devolve ACK de MAC
// e o firmware dele descarta o pacote (node e gateway só aceitam 16 bytes).
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <U8g2lib.h>
#include "protocol.h"
#include "crc16.h"
#include "stats.h"

#define BTN_PIN    0         // BOOT da DevKit; botão externo em paralelo (→ GND)
#define PING_MS    200
#define DRAW_MS    200
#define STALE_MS   180000    // 3 heartbeats sem ouvir direto: o node não chega aqui
#define MAX_NODES  16

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);   // SDA 21, SCL 22

// ponytail: cópia do inventário (fonte: tools/reservoirs.yaml); node fora da lista sai em hex
static const struct { uint16_t id; const char *alias; } ALIAS[] = {
    {0x7758, "CON"}, {0xC9C4, "CAV"}, {0x2EC4, "CB3"}, {0x9EAC, "CIE"}, {0x3456, "CBIF"},
};

struct Rx { uint8_t src[6]; int8_t rssi; espnow_packet_t pkt; };
static QueueHandle_t rxq;

static NodeStat nodes[MAX_NODES];
static uint8_t  nn;
static uint8_t  gw_mac[6];
static bool     gw_ok;
static uint8_t  sel;                         // 0..nn-1 = node, nn = gateway
enum { S_LIST, S_DETAIL, S_PING };
static uint8_t  screen;
static PingWin  pw;
static volatile int8_t ping_res = -1;        // -1 nada, 0 sem ACK, 1 ACK
static bool     in_flight, down;
static uint32_t t_ping, t_down;

static const char *VSHORT[] = {"...", "OK", "LIM", "REP"};
static const char *VLONG[]  = {"coletando", "OK", "LIMITE", "REPETIDOR"};

static const char *alias_of(uint16_t id) {
    for (auto &a : ALIAS) if (a.id == id) return a.alias;
    return nullptr;
}

static const char *name_of(uint16_t id) {
    static char hex[5];
    if (const char *a = alias_of(id)) return a;
    snprintf(hex, sizeof hex, "%04X", id);
    return hex;
}

static const char *sel_name() { return sel < nn ? name_of(nodes[sel].id) : "GW"; }

static const uint8_t *target() {
    if (sel < nn) return nodes[sel].mac_ok ? nodes[sel].mac : nullptr;
    return gw_ok ? gw_mac : nullptr;
}

// ── Rádio ────────────────────────────────────────────────────────────────────
static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(espnow_packet_t)) return;
    Rx r;
    memcpy(&r.pkt, data, sizeof r.pkt);
    if (r.pkt.version != PROTO_VERSION || r.pkt.crc != crc16_ccitt(data, 14)) return;
    memcpy(r.src, info->src_addr, 6);
    r.rssi = info->rx_ctrl->rssi;
    xQueueSend(rxq, &r, 0);
}

static void on_sent(const wifi_tx_info_t *, esp_now_send_status_t st) {
    ping_res = st == ESP_NOW_SEND_SUCCESS;
}

static NodeStat *find(uint16_t id) {
    for (uint8_t i = 0; i < nn; i++) if (nodes[i].id == id) return &nodes[i];
    if (nn >= MAX_NODES) return nullptr;
    if (sel == nn) sel++;                    // GW selecionado continua selecionado
    nodes[nn] = {};
    nodes[nn].id = id;
    return &nodes[nn++];
}

static void handle(const Rx &r) {
    const espnow_packet_t &p = r.pkt;
    if (p.type >= PKT_CMD_CONFIG && p.type <= PKT_OTA_END) return;   // comando do gateway
    NodeStat *s = find(p.node_id);
    if (!s) return;
    bool direct = is_direct(r.src, p.node_id);
    if (direct) {
        memcpy(s->mac, r.src, 6);
        s->mac_ok = true;
        stat_direct(*s, p.seq, r.rssi, millis());
    } else {
        s->relayed++;
    }
    Serial.printf("RX,%lu,%04X,%s,%02X:%02X:%02X:%02X:%02X:%02X,%d,%u,%u,%u,%d,%u\n",
                  millis(), p.node_id, name_of(p.node_id),
                  r.src[0], r.src[1], r.src[2], r.src[3], r.src[4], r.src[5],
                  direct, p.seq, p.type, p.sensor_id, r.rssi, stat_loss(*s));
}

static void ping_tick() {
    uint32_t now = millis();
    if (ping_res >= 0) {
        ping_push(pw, ping_res);
        Serial.printf("PING,%lu,%s,%d,%u\n", now, sel_name(), ping_res, ping_pct(pw));
        ping_res = -1;
        in_flight = false;
    }
    if (in_flight && now - t_ping > 1000) in_flight = false;           // callback perdido
    if (screen != S_PING || in_flight || now - t_ping < PING_MS) return;
    const uint8_t *m = target();
    if (!m) return;
    if (!esp_now_is_peer_exist(m)) {         // ponytail: ≤17 alvos, cabe no limite de 20 peers
        esp_now_peer_info_t pi = {};
        memcpy(pi.peer_addr, m, 6);
        pi.channel = ESPNOW_CHANNEL;
        esp_now_add_peer(&pi);
    }
    static const uint8_t probe = 0xA5;
    t_ping = now;
    in_flight = esp_now_send(m, &probe, 1) == ESP_OK;
}

// ── Botão: toque = próximo node · 1 s = próxima tela · 3 s = zera ────────────
static void button_tick() {
    bool pressed = digitalRead(BTN_PIN) == LOW;
    if (pressed == down) return;
    uint32_t now = millis();
    down = pressed;
    if (pressed) { t_down = now; return; }
    uint32_t held = now - t_down;
    if (held < 40) return;                   // repique
    if (held >= 3000) {
        for (uint8_t i = 0; i < nn; i++) stat_reset(nodes[i]);
        Serial.println("#RESET");
    } else if (held >= 1000) {
        screen = (screen + 1) % 3;
    } else {
        sel = (sel + 1) % (nn + 1);
    }
    pw = {};
}

// ── Tela ─────────────────────────────────────────────────────────────────────
static void header(const char *title) {
    char b[12];
    if (down) {                              // mostra o que acontece se soltar agora
        uint32_t h = millis() - t_down;
        snprintf(b, sizeof b, ">%s", h >= 3000 ? "ZERAR" : h >= 1000 ? "TELA" : "NODE");
    } else {
        snprintf(b, sizeof b, "ch%d", ESPNOW_CHANNEL);
    }
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(0, 8, title);
    oled.drawStr(128 - oled.getStrWidth(b), 8, b);
    oled.drawHLine(0, 10, 128);
}

static void hold_bar() {
    if (!down) return;
    uint32_t h = millis() - t_down;
    oled.setDrawColor(0);
    oled.drawBox(0, 56, 128, 8);
    oled.setDrawColor(1);
    oled.drawFrame(0, 57, 128, 7);
    oled.drawBox(0, 57, (h > 3000 ? 3000 : h) * 128 / 3000, 7);
    oled.drawVLine(43, 55, 9);               // marca de 1 s
}

static void draw_list() {
    char b[32];
    snprintf(b, sizeof b, "NODES %u", nn);
    header(b);
    if (!nn) {
        oled.drawStr(0, 36, "aguardando pacotes");
        oled.drawStr(0, 47, "(heartbeat ~60 s)");
    }
    uint32_t now = millis();
    uint8_t rows = nn + 1, first = sel > 3 ? sel - 3 : 0;
    for (uint8_t i = first; i < rows && i < first + 4; i++) {
        uint8_t y = 21 + (i - first) * 11;
        if (i == nn) {
            snprintf(b, sizeof b, "GW   %s", gw_ok ? "ping pronto" : "defina GW_MAC");
        } else {
            const NodeStat &s = nodes[i];
            if (!s.hn)
                snprintf(b, sizeof b, "%-4s   -- via %lu", name_of(s.id), (unsigned long)s.relayed);
            else
                snprintf(b, sizeof b, "%-4s %4d %-3s%3u%%%s", name_of(s.id), stat_avg(s),
                         VSHORT[stat_verdict(s)], stat_loss(s),
                         now - s.last_ms > STALE_MS ? " !" : "");
        }
        if (i == sel) { oled.drawBox(0, y - 8, 128, 10); oled.setDrawColor(0); }
        oled.drawStr(1, y, b);
        oled.setDrawColor(1);
    }
}

static void draw_detail() {
    char b[32];
    if (sel >= nn) {
        header("GATEWAY");
        if (gw_ok) {
            snprintf(b, sizeof b, "%02X:%02X:%02X:%02X:%02X:%02X",
                     gw_mac[0], gw_mac[1], gw_mac[2], gw_mac[3], gw_mac[4], gw_mac[5]);
            oled.drawStr(0, 26, b);
        } else {
            oled.drawStr(0, 26, "defina GW_MAC");
        }
        oled.drawStr(0, 40, "segure 1 s: PING");
        return;
    }
    const NodeStat &s = nodes[sel];
    const char *a = alias_of(s.id);
    if (a) snprintf(b, sizeof b, "%s %04X", a, s.id); else snprintf(b, sizeof b, "%04X", s.id);
    header(b);
    if (!s.hn) {
        oled.drawStr(0, 28, "sem pacote direto");
        snprintf(b, sizeof b, "via repetidor: %lu", (unsigned long)s.relayed);
        oled.drawStr(0, 40, b);
        return;
    }
    Verdict v = stat_verdict(s);
    oled.setFont(u8g2_font_logisoso16_tr);
    snprintf(b, sizeof b, "%d", stat_avg(s));
    oled.drawStr(0, 30, b);
    uint8_t x = oled.getStrWidth(b) + 2;
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(x, 30, "dBm");
    oled.drawStr(128 - oled.getStrWidth(VLONG[v]), 21, VLONG[v]);
    snprintf(b, sizeof b, "perda %u%%", stat_loss(s));
    oled.drawStr(128 - oled.getStrWidth(b), 31, b);

    uint32_t age = (millis() - s.last_ms) / 1000;
    snprintf(b, sizeof b, "min%d n%lu %lus%s", stat_min(s), (unsigned long)s.n,
             (unsigned long)(age > 999 ? 999 : age), s.relayed ? " +rep" : "");
    oled.drawStr(0, 42, b);

    // Histórico: barras de -100 (vazio) a -40 dBm (cheio); pontilhado = RSSI_LIM
    uint8_t start = s.hn < HIST ? 0 : s.hi;
    for (uint8_t k = 0; k < s.hn; k++) {
        int r = s.hist[(start + k) % HIST];
        int h = (constrain(r, -100, -40) + 100) * 18 / 60;
        if (h) oled.drawBox(k * 4, 64 - h, 3, h);
    }
    uint8_t ylim = 64 - (RSSI_LIM + 100) * 18 / 60;
    for (uint8_t xx = 0; xx < 128; xx += 4) oled.drawPixel(xx + 3, ylim);
}

static void draw_ping() {
    char b[32];
    snprintf(b, sizeof b, "PING %s", sel_name());
    header(b);
    if (!target()) {
        oled.drawStr(0, 28, sel < nn ? "MAC desconhecido:" : "MAC do gateway:");
        oled.drawStr(0, 40, sel < nn ? "espere pacote direto" : "defina GW_MAC");
        return;
    }
    oled.setFont(u8g2_font_logisoso22_tr);
    snprintf(b, sizeof b, "%u%%", ping_pct(pw));
    oled.drawStr(0, 37, b);
    oled.setFont(u8g2_font_6x10_tf);
    snprintf(b, sizeof b, "%u/%u", __builtin_popcount(pw.bits), pw.n);
    oled.drawStr(128 - oled.getStrWidth(b), 23, b);
    oled.drawStr(128 - oled.getStrWidth("entregue"), 34, "entregue");

    for (uint8_t k = 0; k < pw.n; k++) {     // mais recente à direita
        uint8_t x = 122 - k * 6;
        if ((pw.bits >> k) & 1) oled.drawBox(x, 42, 5, 7); else oled.drawFrame(x, 42, 5, 7);
    }
    if (sel < nn && nodes[sel].hn) {
        const NodeStat &s = nodes[sel];
        snprintf(b, sizeof b, "rx %d  perda %u%%", s.hist[(s.hi + HIST - 1) % HIST], stat_loss(s));
    } else {
        snprintf(b, sizeof b, "c/ retry: otimista");
    }
    oled.drawStr(0, 62, b);
}

static void draw() {
    oled.clearBuffer();
    if (screen == S_LIST) draw_list(); else if (screen == S_DETAIL) draw_detail(); else draw_ping();
    hold_bar();
    oled.sendBuffer();
}

void setup() {
    Serial.begin(115200);
    pinMode(BTN_PIN, INPUT_PULLUP);
    oled.setBusClock(400000);
    oled.begin();
    oled.setFontMode(1);
    rxq = xQueueCreate(32, sizeof(Rx));

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);                    // modem sleep perde broadcast e vira "perda" falsa
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) {
        oled.clearBuffer();
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(0, 30, "ESP-NOW falhou");
        oled.drawStr(0, 42, "reinicie a placa");
        oled.sendBuffer();
        Serial.println("#ERRO esp_now_init");
        for (;;) delay(1000);
    }
    esp_now_register_recv_cb(on_recv);
    esp_now_register_send_cb(on_sent);
#ifdef GW_MAC
    const uint8_t g[6] = GW_MAC;
    memcpy(gw_mac, g, 6);
    gw_ok = true;
#endif
    Serial.printf("#BOOT espnow_survey ch=%d mac=%s\n", ESPNOW_CHANNEL, WiFi.macAddress().c_str());
    Serial.println("#RX,ms,node,alias,src_mac,direto,seq,type,sensor_id,rssi,perda%");
    Serial.println("#PING,ms,alvo,ack,entrega%");
}

void loop() {
    Rx r;
    while (xQueueReceive(rxq, &r, 0) == pdTRUE) handle(r);
    button_tick();
    ping_tick();
    static uint32_t t_draw;
    if (millis() - t_draw >= (down ? 50u : DRAW_MS)) {   // barra do botão anima mais rápido
        t_draw = millis();
        draw();
    }
    delay(2);
}
