#include "alarm.h"
#include "comm_config.h"
#include "protocol.h"
#include "crc16.h"
#include "voice_net.h"
#include <Arduino.h>
#include <esp_now.h>
#include <esp_log.h>

static const char *TAG = "alarm";

static const uint8_t BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static uint16_t s_node = 0;
static uint16_t s_seq = 0;
static uint8_t  s_code = ALARM_CLEAR;
static uint16_t s_origin = 0;
static uint32_t s_siren_since = 0;
static bool     s_siren = false;

// Repetição do último evento local (broadcast sem ACK): 3× a 200 ms.
static uint8_t  s_tx_left = 0;
static uint32_t s_tx_last = 0;
static uint16_t s_tx_seq = 0;
static uint8_t  s_tx_code = 0;

// Dedup de eventos remotos (o mesmo chega por ESP-NOW, UDP e repetições).
static struct { uint16_t node, seq; } s_seen[8];
static uint8_t s_seen_i = 0;

static void siren(bool on) {
    if (on && !s_siren) s_siren_since = millis();
    s_siren = on;
    if (COMM_PIN_SIREN >= 0) digitalWrite(COMM_PIN_SIREN, on ? HIGH : LOW);   // -1 = só tom no falante
}

static void send_espnow(uint16_t seq, uint8_t code) {
    espnow_packet_t p = {};
    p.version     = PROTO_VERSION;
    p.type        = PKT_HEARTBEAT;
    p.node_id     = s_node;
    p.sensor_id   = SENSOR_ID_ALARM;
    p.ttl         = DEFAULT_TTL;
    p.seq         = seq;
    p.distance_cm = code;
    p.vbat        = -1;
    p.reserved    = COMM_ZONE;
    p.crc         = crc16_ccitt((const uint8_t *)&p, 14);
    esp_now_send(BCAST, (const uint8_t *)&p, sizeof(p));
}

static void on_espnow(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(espnow_packet_t)) return;
    const espnow_packet_t *p = (const espnow_packet_t *)data;
    if (p->version != PROTO_VERSION || p->type != PKT_HEARTBEAT ||
        p->sensor_id != SENSOR_ID_ALARM) return;
    if (crc16_ccitt(data, 14) != p->crc) return;
    alarm_on_remote(p->node_id, p->seq, (uint8_t)p->distance_cm, p->reserved);
}

static void apply(uint8_t code, uint16_t origin) {
    s_code = code;
    s_origin = origin;
    siren(code != ALARM_CLEAR && code != ALARM_TEST);
    ESP_LOGW(TAG, "alarme=%u origem=0x%04X", code, origin);
}

static void emit(uint8_t code) {
    s_tx_seq = ++s_seq;
    s_tx_code = code;
    s_tx_left = 3;
    s_tx_last = 0;               // primeira já no próximo tick
}

void alarm_init(uint16_t node_id) {
    s_node = node_id;
    if (COMM_PIN_SIREN >= 0) pinMode(COMM_PIN_SIREN, OUTPUT);
    siren(false);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = 0;            // canal atual (o do AP, ou ESPNOW_CHANNEL sem WiFi)
    peer.ifidx   = WIFI_IF_STA;
    esp_now_add_peer(&peer);
    esp_now_register_recv_cb(on_espnow);
}

void alarm_raise(uint8_t code) { apply(code, s_node); emit(code); }
void alarm_clear(void)         { apply(ALARM_CLEAR, s_node); emit(ALARM_CLEAR); }

void alarm_on_remote(uint16_t node_id, uint16_t seq, uint8_t code, uint8_t zone) {
    if (node_id == s_node) return;
    if (zone != 0 && COMM_ZONE != 0 && zone != COMM_ZONE) return;
    for (auto &e : s_seen) if (e.node == node_id && e.seq == seq) return;
    s_seen[s_seen_i] = { node_id, seq };
    s_seen_i = (s_seen_i + 1) % 8;
    // Limpeza só vale vinda de quem disparou (ou se já estava limpo).
    if (code == ALARM_CLEAR && s_code != ALARM_CLEAR && s_origin != node_id) return;
    apply(code, node_id);
}

void alarm_tick(void) {
    uint32_t now = millis();
    if (s_tx_left && (s_tx_last == 0 || now - s_tx_last >= 200)) {
        send_espnow(s_tx_seq, s_tx_code);
        alarm_send_udp(s_tx_seq, s_tx_code, COMM_ZONE);
        s_tx_last = now;
        s_tx_left--;
    }
    if (s_siren && now - s_siren_since > COMM_SIREN_MAX_S * 1000UL) {
        siren(false);
        ESP_LOGW(TAG, "sirene desligada por tempo (%d s); alarme continua", COMM_SIREN_MAX_S);
    }
}

uint8_t  alarm_code(void)     { return s_code; }
uint16_t alarm_origin(void)   { return s_origin; }
bool     alarm_siren_on(void) { return s_siren; }
