#include "voice_net.h"
#include "comm_config.h"
#include "ulaw.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <esp_log.h>

static const char *TAG = "voice";

static const IPAddress GROUP(239, 255, 42, 99);
static const uint16_t  PORT = 5004;

typedef struct __attribute__((packed)) {
    char     magic[2];      // "AV" voz | "AC" alarme
    uint8_t  ver;           // 1
    uint8_t  zone;          // 0 = todas
    uint16_t node_id;
    uint16_t seq;
    uint8_t  flags;         // voz: VOICE_FLAG_* | alarme: código ALARM_*
    uint8_t  prio;
} voice_hdr_t;              // 10 bytes
static_assert(sizeof(voice_hdr_t) == 10, "cabeçalho de voz = 10 B");

#define RING      8192      // amostras (512 ms)
#define PREBUF    1280      // 80 ms antes de começar a tocar
#define RELEASE_MS 600      // solta o locutor depois deste silêncio

static AsyncUDP      s_udp;
static bool          s_ready = false;
static uint16_t      s_node = 0;
static uint16_t      s_seq = 0;
static alarm_rx_cb_t s_on_alarm = nullptr;
static volatile bool s_rx_on = true;

static int16_t       s_ring[RING];
static volatile uint32_t s_w = 0, s_r = 0;        // contadores livres (só crescem)
static bool          s_playing = false;
static volatile uint16_t s_talker = 0;
static volatile uint8_t  s_talker_prio = 0;
static volatile uint32_t s_last_rx_ms = 0;
static portMUX_TYPE  s_mux = portMUX_INITIALIZER_UNLOCKED;

static bool zone_match(uint8_t z) {
    return z == 0 || COMM_ZONE == 0 || z == COMM_ZONE;
}

static void on_packet(AsyncUDPPacket &pkt) {
    if (pkt.length() < sizeof(voice_hdr_t)) return;
    const voice_hdr_t *h = (const voice_hdr_t *)pkt.data();
    if (h->ver != 1 || h->node_id == s_node || !zone_match(h->zone)) return;

    if (h->magic[0] == 'A' && h->magic[1] == 'C') {
        if (s_on_alarm) s_on_alarm(h->node_id, h->seq, h->flags, h->zone);
        return;
    }
    if (h->magic[0] != 'A' || h->magic[1] != 'V' || !s_rx_on) return;

    uint32_t now = millis();
    // Trava no locutor atual; prioridade maior toma o canal; canal livre após RELEASE_MS.
    if (s_talker && s_talker != h->node_id) {
        bool expired = now - s_last_rx_ms > RELEASE_MS;
        if (!expired && h->prio <= s_talker_prio) return;
        portENTER_CRITICAL(&s_mux);
        s_r = s_w;                                   // descarta o áudio do anterior
        portEXIT_CRITICAL(&s_mux);
    }
    s_talker = h->node_id;
    s_talker_prio = h->prio;
    s_last_rx_ms = now;

    size_t n = pkt.length() - sizeof(voice_hdr_t);
    const uint8_t *u = pkt.data() + sizeof(voice_hdr_t);
    portENTER_CRITICAL(&s_mux);
    if (s_w - s_r + n > RING) s_r = s_w + n - RING;  // estouro: perde o mais antigo
    for (size_t i = 0; i < n; i++) s_ring[(s_w + i) % RING] = ulaw_decode(u[i]);
    s_w += n;
    portEXIT_CRITICAL(&s_mux);

    if (h->flags & VOICE_FLAG_END) s_last_rx_ms = now - RELEASE_MS + 100;   // solta já
}

void voice_net_begin(uint16_t node_id, alarm_rx_cb_t on_alarm) {
    s_node = node_id;
    s_on_alarm = on_alarm;
    if (!s_udp.listenMulticast(GROUP, PORT)) {
        ESP_LOGE(TAG, "listenMulticast falhou");
        return;
    }
    s_udp.onPacket(on_packet);
    s_ready = true;
    ESP_LOGI(TAG, "multicast %s:%u zona=%u prio=%u", GROUP.toString().c_str(), PORT,
             COMM_ZONE, COMM_PRIO);
}

bool voice_net_ready(void) { return s_ready && WiFi.isConnected(); }

void voice_send(const int16_t *pcm, size_t n, bool end) {
    if (!voice_net_ready()) return;
    static uint8_t pkt[sizeof(voice_hdr_t) + AUDIO_FRAME];
    if (n > AUDIO_FRAME) n = AUDIO_FRAME;
    voice_hdr_t *h = (voice_hdr_t *)pkt;
    *h = { {'A', 'V'}, 1, COMM_ZONE, s_node, s_seq++, (uint8_t)(end ? VOICE_FLAG_END : 0),
           COMM_PRIO };
    for (size_t i = 0; i < n; i++) pkt[sizeof(voice_hdr_t) + i] = ulaw_encode(pcm[i]);
    s_udp.writeTo(pkt, sizeof(voice_hdr_t) + n, GROUP, PORT);
}

void alarm_send_udp(uint16_t seq, uint8_t code, uint8_t zone) {
    if (!voice_net_ready()) return;
    voice_hdr_t h = { {'A', 'C'}, 1, zone, s_node, seq, code, COMM_PRIO };
    s_udp.writeTo((const uint8_t *)&h, sizeof(h), GROUP, PORT);
}

bool voice_pop(int16_t *out, size_t n) {
    uint32_t avail = s_w - s_r;
    if (s_talker && millis() - s_last_rx_ms > RELEASE_MS && avail < n) {
        s_talker = 0;                                // locutor saiu
        s_playing = false;
    }
    if (!s_playing) {
        if (avail < PREBUF) return false;
        s_playing = true;
    }
    if (avail < n) {                                 // faltou pacote: rebufferiza
        s_playing = false;
        return false;
    }
    portENTER_CRITICAL(&s_mux);
    for (size_t i = 0; i < n; i++) out[i] = s_ring[(s_r + i) % RING];
    s_r += n;
    portEXIT_CRITICAL(&s_mux);
    return true;
}

uint16_t voice_talker(void) { return s_talker; }

void voice_rx_enable(bool on) {
    s_rx_on = on;
    if (!on) {
        portENTER_CRITICAL(&s_mux);
        s_r = s_w;
        portEXIT_CRITICAL(&s_mux);
        s_talker = 0;
        s_playing = false;
    }
}
