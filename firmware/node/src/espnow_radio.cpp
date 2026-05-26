#include "espnow_radio.h"
#include "crc16.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "espnow";

static uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static recv_callback_t s_recv_cb = nullptr;
static send_done_callback_t s_send_done_cb = nullptr;

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != sizeof(espnow_packet_t)) return;

    espnow_packet_t pkt;
    memcpy(&pkt, data, sizeof(pkt));

    // Verify CRC
    uint16_t expected = crc16_ccitt((const uint8_t *)&pkt, 14);
    if (pkt.crc != expected) {
        ESP_LOGW(TAG, "CRC mismatch: got 0x%04X expected 0x%04X", pkt.crc, expected);
        return;
    }

    // Fill RSSI from radio metadata
    pkt.rssi = (int8_t)info->rx_ctrl->rssi;

    if (s_recv_cb) s_recv_cb(&pkt, info->src_addr);
}

// ESP-IDF v5+: send cb receives wifi_tx_info_t* instead of uint8_t* MAC
static void on_send(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (s_send_done_cb) s_send_done_cb(status == ESP_NOW_SEND_SUCCESS);
}

esp_err_t espnow_init(uint8_t channel, recv_callback_t recv_cb, send_done_callback_t send_done_cb) {
    s_recv_cb      = recv_cb;
    s_send_done_cb = send_done_cb;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
        return err;
    }

    esp_now_register_recv_cb(on_recv);
    esp_now_register_send_cb(on_send);

    // Add broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcast_mac, 6);
    peer.channel = channel;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    ESP_LOGI(TAG, "ESP-NOW init OK, channel %d", channel);
    return ESP_OK;
}

void espnow_fill_crc(espnow_packet_t *pkt) {
    pkt->crc = crc16_ccitt((const uint8_t *)pkt, 14);
}

esp_err_t espnow_send(const espnow_packet_t *pkt) {
    espnow_packet_t p = *pkt;
    p.crc = crc16_ccitt((const uint8_t *)&p, 14);
    return esp_now_send(broadcast_mac, (const uint8_t *)&p, sizeof(p));
}

esp_err_t espnow_send_to(const espnow_packet_t *pkt, const uint8_t *mac) {
    // Ensure peer exists
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, mac, 6);
        peer.channel = ESPNOW_CHANNEL;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }
    espnow_packet_t p = *pkt;
    p.crc = crc16_ccitt((const uint8_t *)&p, 14);
    return esp_now_send(mac, (const uint8_t *)&p, sizeof(p));
}
