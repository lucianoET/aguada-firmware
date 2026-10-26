// Estatística de enlace por node: sem Arduino, para rodar também no PC (check_stats.cpp).
#pragma once
#include <stdint.h>
#include <string.h>

// Limiares do veredito — calibrar em campo. ESP-NOW a 1 Mbps morre em ~-97 dBm;
// -85 deixa ~12 dB de margem para desvanecimento (folhagem molhada, gente passando).
#ifndef RSSI_OK
#define RSSI_OK   -75   // dBm (média)
#endif
#ifndef RSSI_LIM
#define RSSI_LIM  -85
#endif
#ifndef LOSS_OK
#define LOSS_OK   5     // %
#endif
#ifndef LOSS_LIM
#define LOSS_LIM  20
#endif
#define MIN_SAMPLES 3   // abaixo disso o veredito é "coletando"
#define HIST        32  // últimos RSSI diretos guardados

enum Verdict : uint8_t { V_WAIT, V_OK, V_LIM, V_REP };

struct NodeStat {
    uint16_t id;
    uint8_t  mac[6];
    bool     mac_ok;
    uint16_t first_seq, last_seq;
    uint32_t n;          // pacotes diretos únicos desde o último reset
    uint32_t relayed;    // pacotes do node que chegaram via repetidor
    int8_t   hist[HIST];
    uint8_t  hn, hi;     // quantos válidos em hist / próxima posição
    uint32_t last_ms;
};

// Node ID = 2 últimos bytes do MAC: se o transmissor é o próprio node, o quadro veio direto.
static inline bool is_direct(const uint8_t *src, uint16_t node_id) {
    return (uint16_t)((src[4] << 8) | src[5]) == node_id;
}

static inline void stat_direct(NodeStat &s, uint16_t seq, int8_t rssi, uint32_t now) {
    if (s.n && seq == s.last_seq) return;                        // duplicata
    if (!s.n || (uint16_t)(seq - s.last_seq) > 1000) {           // 1º pacote ou node reiniciou
        s.first_seq = seq;
        s.n = 0;
    }
    s.last_seq = seq;
    s.n++;
    s.hist[s.hi] = rssi;
    s.hi = (s.hi + 1) % HIST;
    if (s.hn < HIST) s.hn++;
    s.last_ms = now;
}

// Perda pelos buracos de seq. ponytail: não vê perdas depois do último pacote — a idade cobre.
static inline uint8_t stat_loss(const NodeStat &s) {
    if (!s.n) return 0;
    uint32_t expected = (uint16_t)(s.last_seq - s.first_seq) + 1u;
    return (uint8_t)(100 - s.n * 100 / expected);
}

static inline int8_t stat_avg(const NodeStat &s) {
    if (!s.hn) return 0;
    int32_t sum = 0;
    for (uint8_t i = 0; i < s.hn; i++) sum += s.hist[i];
    return (int8_t)(sum / s.hn);
}

static inline int8_t stat_min(const NodeStat &s) {
    int8_t m = 0;
    for (uint8_t i = 0; i < s.hn; i++) if (!i || s.hist[i] < m) m = s.hist[i];
    return m;
}

// O pior entre RSSI médio e perda.
static inline Verdict stat_verdict(const NodeStat &s) {
    if (s.n < MIN_SAMPLES) return V_WAIT;
    int8_t r = stat_avg(s);
    uint8_t l = stat_loss(s);
    Verdict vr = r >= RSSI_OK ? V_OK : r >= RSSI_LIM ? V_LIM : V_REP;
    Verdict vl = l < LOSS_OK ? V_OK : l <= LOSS_LIM ? V_LIM : V_REP;
    return vr > vl ? vr : vl;
}

// Zera as medições e guarda id/MAC (o ping continua disponível no ponto novo).
static inline void stat_reset(NodeStat &s) {
    NodeStat k = {};
    k.id = s.id;
    memcpy(k.mac, s.mac, 6);
    k.mac_ok = s.mac_ok;
    s = k;
}

// Janela dos últimos 20 pings (bit 1 = ACK recebido).
#define PING_WIN 20
struct PingWin { uint32_t bits; uint8_t n; };

static inline void ping_push(PingWin &w, bool ok) {
    w.bits = ((w.bits << 1) | (ok ? 1u : 0u)) & ((1u << PING_WIN) - 1);
    if (w.n < PING_WIN) w.n++;
}

static inline uint8_t ping_pct(const PingWin &w) {
    return w.n ? (uint8_t)(__builtin_popcount(w.bits) * 100 / w.n) : 0;
}
