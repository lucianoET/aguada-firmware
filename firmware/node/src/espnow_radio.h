#pragma once
#include "protocol.h"
#include <esp_err.h>
#include <stdint.h>

typedef void (*recv_callback_t)(const espnow_packet_t *pkt, const uint8_t *src_mac);
typedef void (*send_done_callback_t)(bool success);

esp_err_t espnow_init(uint8_t channel, recv_callback_t recv_cb, send_done_callback_t send_done_cb);
void      espnow_fill_crc(espnow_packet_t *pkt);
esp_err_t espnow_send(const espnow_packet_t *pkt);
esp_err_t espnow_send_to(const espnow_packet_t *pkt, const uint8_t *mac);
