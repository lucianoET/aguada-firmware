#pragma once
#include "../../shared/protocol.h"
#include <esp_err.h>

typedef void (*gw_recv_cb_t)(const espnow_packet_t *pkt, const uint8_t *src_mac);

typedef struct {
	uint32_t rx_packets;
	uint32_t crc_failures;
} gw_espnow_stats_t;

esp_err_t gw_espnow_init(uint8_t channel, gw_recv_cb_t recv_cb);
esp_err_t gw_espnow_send(const espnow_packet_t *pkt, const uint8_t *dest_mac);
void gw_espnow_get_stats(gw_espnow_stats_t *stats);
