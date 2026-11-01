#include "espnow_radio.h"
#include "crc16.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <string.h>
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char *TAG = "espnow";

#define GW_TX_WAIT_MS     100   // ACK do MAC chega em poucos ms; folga para retransmissões
#define GW_FAIL_FORGET    3     // falhas unicast seguidas → esquece o gateway (volta ao broadcast)

static uint8_t broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static recv_callback_t s_recv_cb = nullptr;
static send_done_callback_t s_send_done_cb = nullptr;

static uint8_t           s_gw_mac[6];
static volatile bool     s_gw_known = false;
static uint8_t           s_gw_fails = 0;
static SemaphoreHandle_t s_gw_tx_sem = nullptr;
static volatile bool     s_gw_tx_ok = false;

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int data_len) {
    if (data_len != sizeof(espnow_packet_t)) return;

    espnow_packet_t pkt;
    memcpy(&pkt, data, sizeof(pkt));

    // Verify CRC
    uint16_t expected = crc16_ccitt((const uint8_t *)&pkt, 14);
    if (pkt.crc != expected) {
        ESP_LOGW(TAG, "CRC mismatch: got 0x%04X expected 0x%04X", pkt.crc, expected);
        return;
    }

    pkt.rssi = (int8_t)info->rx_ctrl->rssi;
    if (s_recv_cb) s_recv_cb(&pkt, info->src_addr);
}

static void on_send(const wifi_tx_info_t *info, esp_now_send_status_t status) {
    if (s_gw_known && info && info->des_addr && memcmp(info->des_addr, s_gw_mac, 6) == 0) {
        s_gw_tx_ok = (status == ESP_NOW_SEND_SUCCESS);
        xSemaphoreGive(s_gw_tx_sem);
    }
    if (s_send_done_cb) s_send_done_cb(status == ESP_NOW_SEND_SUCCESS);
}

esp_err_t espnow_init(uint8_t channel, recv_callback_t recv_cb, send_done_callback_t send_done_cb) {
    s_recv_cb      = recv_cb;
    s_send_done_cb = send_done_cb;
    if (!s_gw_tx_sem) s_gw_tx_sem = xSemaphoreCreateBinary();

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

#ifdef DEFAULT_GATEWAY_MAC
    unsigned int m[6];
    if (sscanf(DEFAULT_GATEWAY_MAC, "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
        uint8_t mac[6];
        for (int i = 0; i < 6; i++) mac[i] = (uint8_t)m[i];
        espnow_set_gateway(mac);
    } else {
        ESP_LOGW(TAG, "DEFAULT_GATEWAY_MAC inválido: %s", DEFAULT_GATEWAY_MAC);
    }
#endif

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

void espnow_set_gateway(const uint8_t *mac) {
    if (!mac) {
        if (s_gw_known) ESP_LOGW(TAG, "gateway esquecido, voltando ao broadcast");
        s_gw_known = false;
        return;
    }
    if (s_gw_known && memcmp(mac, s_gw_mac, 6) == 0) return;
    s_gw_known = false;            // on_send não compara contra MAC pela metade
    memcpy(s_gw_mac, mac, 6);
    s_gw_fails = 0;
    s_gw_known = true;
    ESP_LOGI(TAG, "gateway %02X:%02X:%02X:%02X:%02X:%02X → unicast",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool espnow_gateway_known(void) {
    return s_gw_known;
}

bool espnow_send_gw(const espnow_packet_t *pkt) {
    if (s_gw_known) {
        xSemaphoreTake(s_gw_tx_sem, 0);   // descarta confirmação velha
        s_gw_tx_ok = false;
        if (espnow_send_to(pkt, s_gw_mac) == ESP_OK &&
            xSemaphoreTake(s_gw_tx_sem, pdMS_TO_TICKS(GW_TX_WAIT_MS)) == pdTRUE &&
            s_gw_tx_ok) {
            s_gw_fails = 0;
            return true;
        }
        if (++s_gw_fails >= GW_FAIL_FORGET) espnow_set_gateway(nullptr);
        ESP_LOGW(TAG, "unicast sem ACK (falhas=%u), reenviando em broadcast", s_gw_fails);
    }
    espnow_send(pkt);
    return false;
}
