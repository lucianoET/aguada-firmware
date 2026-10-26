// Checagem da lógica de stats.h no PC:
//   g++ -std=c++17 -Isrc check_stats.cpp -o /tmp/check_stats && /tmp/check_stats
#include <assert.h>
#include <stdio.h>
#include "stats.h"

int main() {
    NodeStat s = {};
    s.id = 0x7758;

    const uint8_t con[6]   = {0x20, 0x6E, 0xF1, 0x6B, 0x77, 0x58};
    const uint8_t relay[6] = {0x20, 0x6E, 0xF1, 0x6B, 0x12, 0x34};
    assert(is_direct(con, 0x7758));
    assert(!is_direct(relay, 0x7758));

    // Coletando até MIN_SAMPLES
    stat_direct(s, 100, -70, 0);
    stat_direct(s, 101, -70, 0);
    assert(stat_verdict(s) == V_WAIT);

    // 10 esperados (100..109), 8 recebidos → 20% de perda
    const uint16_t got[] = {102, 103, 105, 107, 108, 109};
    for (uint16_t q : got) stat_direct(s, q, -70, 0);
    assert(s.n == 8 && stat_loss(s) == 20);
    assert(stat_verdict(s) == V_LIM);           // RSSI bom, perda no limite

    stat_direct(s, 109, -70, 0);                // duplicata não conta
    assert(s.n == 8);

    // seq dá a volta em 65535 sem virar "reboot"
    NodeStat w = {};
    stat_direct(w, 65534, -80, 0);
    stat_direct(w, 65535, -80, 0);
    stat_direct(w, 0, -90, 0);
    assert(w.n == 3 && stat_loss(w) == 0);
    assert(stat_avg(w) == -83 && stat_min(w) == -90);
    assert(stat_verdict(w) == V_LIM);

    // Node reiniciou (seq voltou para 0): janela recomeça
    stat_direct(s, 0, -70, 0);
    assert(s.n == 1 && stat_loss(s) == 0);

    // RSSI ruim manda para REP mesmo sem perda
    NodeStat r = {};
    for (uint16_t q = 0; q < 5; q++) stat_direct(r, q, -90, 0);
    assert(stat_verdict(r) == V_REP);

    // Reset guarda MAC
    r.mac_ok = true; r.mac[5] = 0x58; r.id = 1;
    stat_reset(r);
    assert(r.n == 0 && r.hn == 0 && r.mac_ok && r.mac[5] == 0x58 && r.id == 1);

    // Janela de ping: 20 últimos
    PingWin p = {};
    assert(ping_pct(p) == 0);
    for (int i = 0; i < 10; i++) ping_push(p, i % 2);
    assert(p.n == 10 && ping_pct(p) == 50);
    for (int i = 0; i < 30; i++) ping_push(p, true);
    assert(p.n == 20 && ping_pct(p) == 100);

    puts("stats ok");
}
