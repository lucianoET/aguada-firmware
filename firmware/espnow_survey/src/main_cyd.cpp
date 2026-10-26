// Aguada Monitor — espnow_survey no CYD (ESP32-2432S028R: ILI9341 320x240 + toque XPT2046).
// Mesma medição do main.cpp (stats.h): RSSI/perda por node no ponto onde está e ping por
// unicast de 1 byte (ACK de MAC; node e gateway só aceitam 16 bytes e descartam o resto).
// Toda a rede Aguada é broadcast no ch11, então o callback normal do ESP-NOW já ouve tudo.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "protocol.h"
#include "crc16.h"
#include "stats.h"

// ── Placa ────────────────────────────────────────────────────────────────────
#define TS_CS     33         // XPT2046 no VSPI (CLK 25, MISO 39, MOSI 32); TFT no HSPI
#define TS_IRQ    36
#define LED_R     4          // LED RGB ativo em LOW
#define LED_G     16
#define LED_B     17
#define SPK       26         // conector SPEAK (amp onboard)
// Calibração do toque (bruto) — mesma faixa que funciona no mochi_pet nesta unidade
#define TS_X0 200
#define TS_X1 3700
#define TS_Y0 200
#define TS_Y1 3700

#define PING_MS    200
#define DRAW_MS    200
#define STALE_MS   180000    // 3 heartbeats sem ouvir direto: o node não chega aqui
#define MAX_NODES  16
#define HDR        22        // faixa do cabeçalho
#define TAB_Y      200       // barra de abas (4 × 80 px)
#define ROW_H      22
#define LIST_Y     42
#define LIST_ROWS  7
#define LOG_N      11
#define NONE       0xFFFE    // distância ainda não recebida (0xFFFF = erro do sensor)

// RGB565; em sprite 8 bpp (332) estes cinzas ficam neutros
#define C_DIM   0x8410
#define C_PANEL 0x4208
#define C_SEL   0x0010

static TFT_eSPI tft;
static TFT_eSprite spr(&tft);
static SPIClass tsSPI(VSPI);
static XPT2046_Touchscreen ts(TS_CS, TS_IRQ);

// ponytail: cópia do inventário (fonte: tools/reservoirs.yaml); node fora da lista sai em hex
static const struct { uint16_t id; const char *alias; } ALIAS[] = {
    {0x7758, "CON"}, {0xC9C4, "CAV"}, {0x2EC4, "CB3"}, {0x9EAC, "CIE"}, {0x3456, "CBIF"},
    {0xA101, "SRN"},
};

struct Rx { uint8_t src[6]; int8_t rssi; espnow_packet_t pkt; };
struct Last { uint16_t dist[2]; int8_t vbat; uint8_t flags; uint16_t seq; };
static QueueHandle_t rxq;

static NodeStat nodes[MAX_NODES];
static Last     last[MAX_NODES];
static uint8_t  nn;
static uint8_t  gw_mac[6];
static bool     gw_ok;
static int8_t   gw_rssi;                     // último comando do gateway ouvido aqui
static uint32_t gw_ms;
static uint8_t  sel;                         // 0..nn-1 = node, nn = gateway
enum { S_LIST, S_SIG, S_PING, S_LOG };
static uint8_t  screen;
static PingWin  pw;
static volatile int8_t ping_res = -1;        // -1 nada, 0 sem ACK, 1 ACK
static bool     in_flight;
static uint32_t t_ping, rx_total, t_rx;
static bool     muted, log_pause;
static uint32_t beep_until, led_off_until;
static int16_t  fx, fy, fw, fh;              // retângulo do toque (feedback "pressionado")
static uint32_t flash_until;

struct LogLine { char s[44]; uint16_t c; };
static LogLine  logbuf[LOG_N];
static uint8_t  log_head, log_n;

static const char *VNAME[] = {"...", "OK", "LIMITE", "REPET."};
static const uint16_t VCOL[] = {C_DIM, TFT_GREEN, TFT_YELLOW, TFT_RED};
static const char *TABS[] = {"LISTA", "SINAL", "PING", "LOG"};

static const char *alias_of(uint16_t id) {
    for (auto &a : ALIAS) if (a.id == id) return a.alias;
    return nullptr;
}

static const char *name_of(uint16_t id) {
    static char hex[2][5];
    static uint8_t k;
    if (const char *a = alias_of(id)) return a;
    k ^= 1;                                  // 2 buffers: dá para usar duas vezes no mesmo printf
    snprintf(hex[k], sizeof hex[k], "%04X", id);
    return hex[k];
}

static const char *sel_name() { return sel < nn ? name_of(nodes[sel].id) : "GW"; }

static const uint8_t *target() {
    if (sel < nn) return nodes[sel].mac_ok ? nodes[sel].mac : nullptr;
    return gw_ok ? gw_mac : nullptr;
}

static uint16_t rssi_col(int r) { return r >= RSSI_OK ? TFT_GREEN : r >= RSSI_LIM ? TFT_YELLOW : TFT_RED; }
static uint16_t pct_col(int p)  { return p >= 90 ? TFT_GREEN : p >= 60 ? TFT_YELLOW : TFT_RED; }

static void fmt_age(char *b, size_t n, uint32_t ms) {
    uint32_t s = ms / 1000;
    if (s < 100) snprintf(b, n, "%lus", (unsigned long)s);
    else snprintf(b, n, "%lum", (unsigned long)(s / 60 > 999 ? 999 : s / 60));
}

static void fmt_dist(char *b, size_t n, uint16_t d) {
    if (d == NONE) snprintf(b, n, "--");
    else if (d == 0xFFFF) snprintf(b, n, "erro");
    else snprintf(b, n, "%ucm", d);
}

// ── Som / LED ────────────────────────────────────────────────────────────────
static void beep(uint32_t hz, uint32_t ms) {
    if (muted) return;
    ledcWriteTone(SPK, hz);
    beep_until = millis() + ms;
}

static void led(uint16_t c) {                // só as cores dos vereditos: liga/desliga por canal
    digitalWrite(LED_R, !(c == TFT_RED || c == TFT_YELLOW));
    digitalWrite(LED_G, !(c == TFT_GREEN || c == TFT_YELLOW));
    digitalWrite(LED_B, !(c == C_DIM));
}

static void out_tick() {
    uint32_t now = millis();
    if (beep_until && (int32_t)(now - beep_until) >= 0) { ledcWriteTone(SPK, 0); beep_until = 0; }
    uint16_t c = 0;                          // 0 = apagado
    if (screen == S_PING && target() && pw.n) c = pct_col(ping_pct(pw));
    else if (sel < nn && nodes[sel].n) c = VCOL[stat_verdict(nodes[sel])];
    if ((int32_t)(now - led_off_until) < 0) c = 0;                     // piscada = pacote
    led(c);
}

// ── Log (terminal) ───────────────────────────────────────────────────────────
static void log_add(uint16_t c, const char *fmt, ...) {
    if (log_pause) return;
    LogLine &l = logbuf[log_head];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(l.s, sizeof l.s, fmt, ap);
    va_end(ap);
    l.c = c;
    log_head = (log_head + 1) % LOG_N;
    if (log_n < LOG_N) log_n++;
}

static const char *type_name(uint8_t t) {
    switch (t) {
        case PKT_SENSOR: return "SEN";  case PKT_HEARTBEAT: return "HB";
        case PKT_HELLO: return "HELO";  case PKT_CMD_CONFIG: return "CFG";
        case PKT_CMD_RESTART: return "RST"; case PKT_ACK: return "ACK";
        default: return "?";
    }
}

static void log_packet(const Rx &r, bool direct) {
    const espnow_packet_t &p = r.pkt;
    char v[16];
    if (p.type == PKT_HELLO) snprintf(v, sizeof v, "n=%u", p.distance_cm);
    else if (p.flags & FLAG_IS_AIR_QUALITY) snprintf(v, sizeof v, "eCO2 %u", p.distance_cm);
    else if (p.sensor_id == 0xFE) snprintf(v, sizeof v, "%.1fC %u%%", (p.distance_cm - 1000) / 10.0, p.reserved);
    else if (p.sensor_id >= 0xF0) snprintf(v, sizeof v, "c%X %u", p.sensor_id & 0xF, p.distance_cm);
    else fmt_dist(v, sizeof v, p.distance_cm);
    uint32_t s = millis() / 1000;
    uint16_t c = !direct ? TFT_YELLOW : p.type == PKT_SENSOR ? TFT_WHITE
               : p.type == PKT_HELLO ? TFT_CYAN : C_DIM;
    log_add(c, "%02lu:%02lu %-4s %-4s s%u %-9s %4d%s", (unsigned long)(s / 60 % 100),
            (unsigned long)(s % 60), name_of(p.node_id), type_name(p.type), p.sensor_id & 0xF,
            v, r.rssi, direct ? "" : " rep");
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

static int find(uint16_t id) {
    for (uint8_t i = 0; i < nn; i++) if (nodes[i].id == id) return i;
    if (nn >= MAX_NODES) return -1;
    if (sel == nn) sel++;                    // GW selecionado continua selecionado
    nodes[nn] = {};
    nodes[nn].id = id;
    last[nn] = {{NONE, NONE}, -1, 0, 0};
    return nn++;
}

static void handle(const Rx &r) {
    const espnow_packet_t &p = r.pkt;
    rx_total++;
    t_rx = millis();
    if (p.type >= PKT_CMD_CONFIG && p.type <= PKT_OTA_END) {         // comando do gateway
        if (gw_ok && !memcmp(r.src, gw_mac, 6)) { gw_rssi = r.rssi; gw_ms = millis(); }
        log_add(TFT_MAGENTA, "GW %s -> %s  %d", type_name(p.type), name_of(p.node_id), r.rssi);
        return;
    }
    int i = find(p.node_id);
    if (i < 0) return;
    NodeStat *s = &nodes[i];
    bool direct = is_direct(r.src, p.node_id);
    if (direct) {
        memcpy(s->mac, r.src, 6);
        s->mac_ok = true;
        stat_direct(*s, p.seq, r.rssi, millis());
        if (i == sel) {
            led_off_until = millis() + 60;
            if (screen == S_SIG) beep(map(constrain(r.rssi, -100, -40), -100, -40, 400, 2500), 40);
        }
    } else {
        s->relayed++;
    }
    Last &L = last[i];
    if ((p.type == PKT_SENSOR || p.type == PKT_HEARTBEAT) && (p.sensor_id == 1 || p.sensor_id == 2))
        L.dist[p.sensor_id - 1] = p.distance_cm;
    L.vbat = p.vbat;
    L.flags = p.flags;
    L.seq = p.seq;
    log_packet(r, direct);
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
        if (ping_res) beep(2000, 15);
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

// ── Toque ────────────────────────────────────────────────────────────────────
static void flash(int x, int y, int w, int h) {
    fx = x; fy = y; fw = w; fh = h;
    flash_until = millis() + 150;
    beep(3000, 6);
}

static void next_target() {
    sel = (sel + 1) % (nn + 1);
    pw = {};
}

static void on_tap(int x, int y) {
    if (y >= TAB_Y) {
        uint8_t s = x / 80;
        flash(s * 80, TAB_Y, 80, 240 - TAB_Y);
        if (s != screen) pw = {};
        screen = s;
        return;
    }
    if (y < HDR) {
        if (x >= 250) { muted = !muted; if (muted) ledcWriteTone(SPK, 0); flash(250, 0, 70, HDR); }
        return;
    }
    switch (screen) {
    case S_LIST: {
        uint8_t rows = nn + 1, first = sel >= LIST_ROWS ? sel - LIST_ROWS + 1 : 0;
        if (y < LIST_Y) return;
        uint8_t i = first + (y - LIST_Y) / ROW_H;
        if (i >= rows) return;
        flash(0, LIST_Y + (i - first) * ROW_H, 320, ROW_H);
        if (i == sel) { screen = i < nn ? S_SIG : S_PING; pw = {}; }   // 2º toque abre
        else { sel = i; pw = {}; }
        break;
    }
    case S_SIG:
        if (y < 52 && x >= 246) {
            for (uint8_t i = 0; i < nn; i++) stat_reset(nodes[i]);
            pw = {};
            Serial.println("#RESET");
            flash(246, 26, 70, 24);
        } else if (y < 52) { next_target(); flash(0, HDR, 240, 30); }
        break;
    case S_PING:
        if (y < 52) { next_target(); flash(0, HDR, 320, 30); }
        break;
    case S_LOG:
        log_pause = !log_pause;
        flash(0, HDR, 320, TAB_Y - HDR);
        break;
    }
}

static void touch_tick() {
    static bool down;
    static uint32_t t_up;
    bool t = ts.touched();
    uint32_t now = millis();
    if (!t) {
        if (down && now - t_up > 80) down = false;                   // soltou (com folga p/ repique)
        return;
    }
    t_up = now;
    if (down) return;
    down = true;
    TS_Point p = ts.getPoint();
    int x = map(constrain(p.x, TS_X0, TS_X1), TS_X0, TS_X1, 0, 319);
    int y = map(constrain(p.y, TS_Y0, TS_Y1), TS_Y0, TS_Y1, 0, 239);
    Serial.printf("#TOQUE bruto=%d,%d tela=%d,%d\n", p.x, p.y, x, y);
    on_tap(x, y);
}

// ── Tela ─────────────────────────────────────────────────────────────────────
static void txt(const char *s, int x, int y, uint8_t font, uint16_t c, uint8_t datum = TL_DATUM,
                uint16_t bg = TFT_BLACK) {
    spr.setTextDatum(datum);
    spr.setTextColor(c, bg);
    spr.drawString(s, x, y, font);
}

static void chip(const char *s, int x, int y, int w, int h, uint16_t bg, uint8_t font = 2) {
    spr.fillRoundRect(x, y, w, h, 4, bg);
    txt(s, x + w / 2, y + h / 2 + 1, font, TFT_BLACK, MC_DATUM, bg);
}

static void draw_header() {
    char b[24];
    spr.fillRect(0, 0, 320, HDR, C_PANEL);
    snprintf(b, sizeof b, "AGUADA ch%d", ESPNOW_CHANNEL);
    txt(b, 4, 3, 2, TFT_WHITE, TL_DATUM, C_PANEL);
    bool act = millis() - t_rx < 150;
    spr.fillCircle(118, 11, 4, act ? TFT_GREEN : TFT_BLACK);
    snprintf(b, sizeof b, "rx %lu", (unsigned long)rx_total);
    txt(b, 128, 3, 2, TFT_WHITE, TL_DATUM, C_PANEL);
    if (log_pause) txt("PAUSA", 238, 3, 2, TFT_YELLOW, TR_DATUM, C_PANEL);
    chip(muted ? "MUDO" : "SOM", 254, 2, 62, 18, muted ? TFT_RED : TFT_GREEN);
}

static void draw_tabs() {
    for (uint8_t i = 0; i < 4; i++) {
        bool on = i == screen;
        uint16_t bg = on ? TFT_BLUE : C_PANEL;
        spr.fillRect(i * 80 + 1, TAB_Y + 1, 78, 239 - TAB_Y, bg);
        txt(TABS[i], i * 80 + 40, TAB_Y + 21, 4, on ? TFT_WHITE : C_DIM, MC_DATUM, bg);
    }
}

static void draw_list() {
    char b[16];
    txt("NODE", 4, 26, 2, C_DIM);
    txt("dBm", 92, 26, 2, C_DIM, TR_DATUM);
    txt("ESTADO", 100, 26, 2, C_DIM);
    txt("PERDA", 196, 26, 2, C_DIM, TR_DATUM);
    txt("IDADE", 248, 26, 2, C_DIM, TR_DATUM);
    txt("s1", 316, 26, 2, C_DIM, TR_DATUM);
    spr.drawFastHLine(0, 40, 320, C_PANEL);
    if (!nn) {
        txt("aguardando pacotes...", 160, 100, 4, TFT_WHITE, MC_DATUM);
        txt("heartbeat dos nodes ~60 s", 160, 130, 2, C_DIM, MC_DATUM);
    }
    uint32_t now = millis();
    uint8_t rows = nn + 1, first = sel >= LIST_ROWS ? sel - LIST_ROWS + 1 : 0;
    for (uint8_t i = first; i < rows && i < first + LIST_ROWS; i++) {
        int y = LIST_Y + (i - first) * ROW_H;
        uint16_t bg = i == sel ? C_SEL : TFT_BLACK;
        spr.fillRect(0, y, 320, ROW_H, bg);
        int cy = y + ROW_H / 2 + 1;
        if (i == nn) {
            txt("GW", 4, cy, 4, TFT_CYAN, ML_DATUM, bg);
            if (gw_ms) {
                snprintf(b, sizeof b, "%d", gw_rssi);
                txt(b, 92, cy, 2, rssi_col(gw_rssi), MR_DATUM, bg);
                fmt_age(b, sizeof b, now - gw_ms);
                txt(b, 248, cy, 2, C_DIM, MR_DATUM, bg);
            }
            txt(gw_ok ? "PING >" : "sem GW_MAC", 100, cy, 2, C_DIM, ML_DATUM, bg);
            continue;
        }
        const NodeStat &s = nodes[i];
        txt(name_of(s.id), 4, cy, 4, TFT_WHITE, ML_DATUM, bg);
        if (!s.hn) {
            snprintf(b, sizeof b, "so via rep %lu", (unsigned long)s.relayed);
            txt(b, 100, cy, 2, TFT_YELLOW, ML_DATUM, bg);
        } else {
            int8_t a = stat_avg(s);
            snprintf(b, sizeof b, "%d", a);
            txt(b, 92, cy, 4, rssi_col(a), MR_DATUM, bg);
            Verdict v = stat_verdict(s);
            chip(VNAME[v], 100, y + 3, 62, ROW_H - 6, VCOL[v]);
            snprintf(b, sizeof b, "%u%%", stat_loss(s));
            txt(b, 196, cy, 2, TFT_WHITE, MR_DATUM, bg);
            bool stale = now - s.last_ms > STALE_MS;
            fmt_age(b, sizeof b, now - s.last_ms);
            txt(b, 248, cy, 2, stale ? TFT_RED : C_DIM, MR_DATUM, bg);
        }
        fmt_dist(b, sizeof b, last[i].dist[0]);
        txt(b, 316, cy, 2, TFT_WHITE, MR_DATUM, bg);
    }
    if (rows > LIST_ROWS) txt("toque 2x abre", 316, 196, 1, C_DIM, BR_DATUM);
}

static void draw_sig() {
    char b[32];
    if (sel >= nn) {
        txt("GATEWAY", 8, 28, 4, TFT_CYAN);
        txt("O gateway so transmite comando.", 8, 70, 2, TFT_WHITE);
        txt("Use a aba PING para medir o enlace", 8, 90, 2, TFT_WHITE);
        txt("ate ele daqui (ACK de MAC).", 8, 110, 2, TFT_WHITE);
        if (gw_ms) {
            snprintf(b, sizeof b, "ultimo comando ouvido: %d dBm", gw_rssi);
            txt(b, 8, 140, 2, rssi_col(gw_rssi));
        }
        txt("toque no titulo: proximo", 8, 180, 2, C_DIM);
        return;
    }
    const NodeStat &s = nodes[sel];
    const Last &L = last[sel];
    snprintf(b, sizeof b, "%s  %04X  >", name_of(s.id), s.id);
    txt(b, 8, 28, 4, TFT_WHITE);
    spr.fillRoundRect(246, 26, 70, 24, 4, C_PANEL);
    txt("ZERAR", 281, 39, 2, TFT_WHITE, MC_DATUM, C_PANEL);
    if (!s.hn) {
        txt("sem pacote direto aqui", 8, 80, 4, TFT_YELLOW);
        snprintf(b, sizeof b, "chegou via repetidor: %lu", (unsigned long)s.relayed);
        txt(b, 8, 115, 2, TFT_WHITE);
        return;
    }
    Verdict v = stat_verdict(s);
    int8_t a = stat_avg(s);
    snprintf(b, sizeof b, "%d", a);
    spr.setTextColor(rssi_col(a), TFT_BLACK);
    spr.setTextDatum(TL_DATUM);
    int w = spr.drawString(b, 8, 58, 7);
    txt("dBm", 12 + w, 58, 2, C_DIM);
    txt("media", 12 + w, 76, 2, C_DIM);
    chip(VNAME[v], 200, 56, 116, 26, VCOL[v], 4);
    uint32_t age = millis() - s.last_ms;
    char ag[8];
    fmt_age(ag, sizeof ag, age);
    snprintf(b, sizeof b, "perda %u%%   n %lu", stat_loss(s), (unsigned long)s.n);
    txt(b, 316, 88, 2, TFT_WHITE, TR_DATUM);
    snprintf(b, sizeof b, "ult %d  min %d  %s", s.hist[(s.hi + HIST - 1) % HIST], stat_min(s), ag);
    txt(b, 316, 104, 2, age > STALE_MS ? TFT_RED : C_DIM, TR_DATUM);

    char d1[8], d2[8];
    fmt_dist(d1, sizeof d1, L.dist[0]);
    fmt_dist(d2, sizeof d2, L.dist[1]);
    if (L.vbat >= 0) snprintf(b, sizeof b, "s1 %s  s2 %s  %d.%dV", d1, d2, L.vbat / 10, L.vbat % 10);
    else snprintf(b, sizeof b, "s1 %s  s2 %s  seq %u", d1, d2, L.seq);
    txt(b, 8, 120, 2, TFT_WHITE);
    if (s.relayed) {
        snprintf(b, sizeof b, "+%lu via rep", (unsigned long)s.relayed);
        txt(b, 316, 120, 2, TFT_YELLOW, TR_DATUM);
    }

    // Histórico: -100 (vazio) a -40 dBm (cheio), mais recente à direita; linhas = OK e LIMITE
    const int gy = 142, gh = 54;
    uint8_t start = s.hn < HIST ? 0 : s.hi;
    for (uint8_t k = 0; k < s.hn; k++) {
        int r = s.hist[(start + k) % HIST];
        int h = (constrain(r, -100, -40) + 100) * gh / 60;
        int x = 8 + (HIST - s.hn + k) * 9;
        if (h) spr.fillRect(x, gy + gh - h, 7, h, rssi_col(r));
    }
    for (int lim : {RSSI_OK, RSSI_LIM}) {
        int yl = gy + gh - (lim + 100) * gh / 60;
        for (int x = 8; x < 296; x += 6) spr.drawFastHLine(x, yl, 3, lim == RSSI_OK ? TFT_GREEN : TFT_RED);
    }
}

static void draw_ping() {
    char b[32];
    snprintf(b, sizeof b, "PING %s  >", sel_name());
    txt(b, 8, 28, 4, TFT_WHITE);
    const uint8_t *m = target();
    if (!m) {
        txt(sel < nn ? "MAC desconhecido:" : "MAC do gateway:", 8, 80, 4, TFT_YELLOW);
        txt(sel < nn ? "espere um pacote direto dele" : "defina GW_MAC no platformio.ini", 8, 112, 2, TFT_WHITE);
        return;
    }
    snprintf(b, sizeof b, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
    txt(b, 316, 32, 2, C_DIM, TR_DATUM);
    uint8_t pct = ping_pct(pw);
    snprintf(b, sizeof b, "%u", pct);
    spr.setTextColor(pw.n ? pct_col(pct) : C_DIM, TFT_BLACK);
    spr.setTextDatum(TL_DATUM);
    int w = spr.drawString(b, 8, 60, 7);
    txt("%", 12 + w, 60, 4, C_DIM);
    snprintf(b, sizeof b, "%u/%u", __builtin_popcount(pw.bits), pw.n);
    txt(b, 316, 62, 4, TFT_WHITE, TR_DATUM);
    txt("entregue (ACK)", 316, 90, 2, C_DIM, TR_DATUM);
    for (uint8_t k = 0; k < pw.n; k++) {     // mais recente à direita
        int x = 301 - k * 15;
        if ((pw.bits >> k) & 1) spr.fillRect(x, 124, 12, 18, TFT_GREEN);
        else spr.drawRect(x, 124, 12, 18, TFT_RED);
    }
    if (sel < nn && nodes[sel].hn) {
        const NodeStat &s = nodes[sel];
        snprintf(b, sizeof b, "rx dele aqui: %d dBm  perda %u%%", s.hist[(s.hi + HIST - 1) % HIST], stat_loss(s));
    } else {
        snprintf(b, sizeof b, "ACK conta retry: resultado otimista");
    }
    txt(b, 8, 160, 2, C_DIM);
    txt("toque no titulo: proximo alvo", 8, 178, 2, C_DIM);
}

static void draw_log() {
    if (!log_n) txt("sem pacotes ainda", 160, 100, 4, C_DIM, MC_DATUM);
    uint8_t first = (log_head + LOG_N - log_n) % LOG_N;
    for (uint8_t k = 0; k < log_n; k++) {
        const LogLine &l = logbuf[(first + k) % LOG_N];
        txt(l.s, 2, HDR + 2 + k * 16, 2, l.c);
    }
}

static void draw() {
    spr.fillSprite(TFT_BLACK);
    draw_header();
    if (screen == S_LIST) draw_list();
    else if (screen == S_SIG) draw_sig();
    else if (screen == S_PING) draw_ping();
    else draw_log();
    draw_tabs();
    if ((int32_t)(millis() - flash_until) < 0) {                      // feedback de toque
        spr.drawRect(fx, fy, fw, fh, TFT_WHITE);
        spr.drawRect(fx + 1, fy + 1, fw - 2, fh - 2, TFT_WHITE);
    }
    spr.pushSprite(0, 0);
}

void setup() {
    Serial.begin(115200);
    for (int p : {LED_R, LED_G, LED_B}) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); }
    ledcAttach(SPK, 2000, 8);
    ledcWriteTone(SPK, 0);
    tft.init();
    tft.setRotation(1);                      // paisagem 320x240
    tft.fillScreen(TFT_BLACK);
    tsSPI.begin(25, 39, 32, TS_CS);
    ts.begin(tsSPI);
    ts.setRotation(1);
    spr.setColorDepth(8);                    // 77 kB; 16 bpp (154 kB) não cabe com o Wi-Fi
    if (!spr.createSprite(320, 240)) {       // antes do Wi-Fi, enquanto o heap está inteiro
        tft.drawString("sem RAM para a tela", 10, 100, 4);
        Serial.println("#ERRO createSprite");
        for (;;) delay(1000);
    }
    rxq = xQueueCreate(32, sizeof(Rx));

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setSleep(false);                    // modem sleep perde broadcast e vira "perda" falsa
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (esp_now_init() != ESP_OK) {
        tft.drawString("ESP-NOW falhou - reinicie", 10, 100, 4);
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
    uint8_t me[6];
    esp_wifi_get_mac(WIFI_IF_STA, me);       // WiFi.macAddress() sai zerado sem conectar (core 3.x)
    Serial.printf("#BOOT aguada_monitor_cyd ch=%d mac=%02X:%02X:%02X:%02X:%02X:%02X heap=%u\n",
                  ESPNOW_CHANNEL, me[0], me[1], me[2], me[3], me[4], me[5], ESP.getFreeHeap());
    Serial.println("#RX,ms,node,alias,src_mac,direto,seq,type,sensor_id,rssi,perda%");
    Serial.println("#PING,ms,alvo,ack,entrega%");
    beep(1500, 60);
}

void loop() {
    Rx r;
    while (xQueueReceive(rxq, &r, 0) == pdTRUE) handle(r);
    touch_tick();
    ping_tick();
    out_tick();
    static uint32_t t_draw;
    if (millis() - t_draw >= DRAW_MS || (int32_t)(millis() - flash_until) < 0) {
        t_draw = millis();
        draw();
    }
    delay(2);
}
