#pragma once
// AP de manutenção do gateway Ethernet (env gateway-wt32-eth).
//
// O AP fica desligado: o botão liga (ou renova) por AP_IDLE_TIMEOUT_MS contados a
// partir da última requisição HTTP. Enquanto ligado, serve uma página única com
// status (link ETH, MQTT, nodes vistos) e o formulário de rede/MQTT/senha do AP,
// gravado em NVS. Só clientes do AP são atendidos: apertar o botão é a autorização.
// IP fixo errado tem saída — o botão sempre abre o AP.
#ifdef GATEWAY_ETH

#include <Arduino.h>
#include <IPAddress.h>
#include <ArduinoJson.h>
#include "../../shared/protocol.h"

struct gw_cfg_t {
    bool      dhcp;
    IPAddress ip, gw, mask, dns;
    char      mqtt_host[64];
    uint16_t  mqtt_port;
    char      mqtt_user[33];
    char      mqtt_pass[64];
    char      ap_pass[64];
};

// main.cpp preenche link ETH, MQTT e contadores; o portal soma AP, config e nodes.
typedef void (*portal_state_cb_t)(JsonDocument &doc);

namespace portal {

// NVS por cima dos defaults de build (secrets.ini). Primeiro boot = só defaults.
void load_cfg(gw_cfg_t &cfg);

// Configura o botão; o AP só sobe no primeiro aperto. `cfg` precisa viver para sempre.
void begin(const gw_cfg_t &cfg, const uint8_t mac[6], portal_state_cb_t state_cb);

// Botão, timeout, DNS e HTTP. Chamar a cada volta do loop(); não bloqueia.
void tick(void);

// Atualiza a tabela de nodes da página (último pacote por node+sensor).
void note_packet(const espnow_packet_t *pkt);

bool ap_active(void);

}  // namespace portal

#endif  // GATEWAY_ETH
