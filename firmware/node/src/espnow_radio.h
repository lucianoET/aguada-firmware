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

// ── Unicast ao gateway (ACK de camada MAC + retransmissão automática) ─────────
// O MAC do gateway vem da flag DEFAULT_GATEWAY_MAC ou é aprendido do ACK que o
// gateway devolve ao HELLO direto. Desconhecido → broadcast, como antes.
void espnow_set_gateway(const uint8_t *mac);   // nullptr = esquecer
bool espnow_gateway_known(void);
// Unicast ao gateway; se a entrega falhar (ou gateway desconhecido) reenvia em
// broadcast para relays/mesh. Retorna true só com entrega unicast confirmada.
bool espnow_send_gw(const espnow_packet_t *pkt);
