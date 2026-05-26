#include "espnow_gw.h"
#include "crc16.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "gw_espnow";
static gw_recv_cb_t s_recv_cb = nullptr;
static uint32_t s_rx_packets = 0;
static uint32_t s_crc_failures = 0;

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(espnow_packet_t)) return;

    espnow_packet_t pkt;
    memcpy(&pkt, data, sizeof(pkt));

    uint16_t expected = crc16_ccitt((const uint8_t *)&pkt, 14);
    if (pkt.crc != expected) {
        s_crc_failures++;
        ESP_LOGW(TAG, "CRC fail node=0x%04X got=0x%04X exp=0x%04X",
                 pkt.node_id, pkt.crc, expected);
        return;
    }

    s_rx_packets++;
    pkt.rssi = (int8_t)info->rx_ctrl->rssi;
    if (s_recv_cb) s_recv_cb(&pkt, info->src_addr);
}

esp_err_t gw_espnow_init(uint8_t channel, gw_recv_cb_t recv_cb) {
    s_recv_cb = recv_cb;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "init failed: %s", esp_err_to_name(err)); return err; }

    esp_now_register_recv_cb(on_recv);
    ESP_LOGI(TAG, "ESP-NOW ready ch=%d", channel);
    return ESP_OK;
}

esp_err_t gw_espnow_send(const espnow_packet_t *pkt, const uint8_t *dest_mac) {
    if (!esp_now_is_peer_exist(dest_mac)) {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, dest_mac, 6);
        peer.channel = ESPNOW_CHANNEL;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }
    espnow_packet_t p = *pkt;
    p.crc = crc16_ccitt((const uint8_t *)&p, 14);
    return esp_now_send(dest_mac, (const uint8_t *)&p, sizeof(p));
}

void gw_espnow_get_stats(gw_espnow_stats_t *stats) {
    if (!stats) return;
    stats->rx_packets = s_rx_packets;
    stats->crc_failures = s_crc_failures;
}
