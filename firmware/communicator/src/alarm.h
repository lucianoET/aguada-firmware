#pragma once
#include <stdint.h>

// Alarme: estado local, sirene, e propagação por ESP-NOW (PKT_HEARTBEAT, SENSOR_ID_ALARM)
// e por multicast. Recebido de outro node (ESP-NOW ou UDP) → mesma reação local.

void    alarm_init(uint16_t node_id);          // depois do WiFi.mode() e esp_now_init()
void    alarm_raise(uint8_t code);             // local: liga e propaga (ALARM_*)
void    alarm_clear(void);                     // local: limpa e propaga
void    alarm_on_remote(uint16_t node_id, uint16_t seq, uint8_t code, uint8_t zone);
void    alarm_tick(void);                      // repetições de TX + timeout da sirene

uint8_t  alarm_code(void);                     // ALARM_CLEAR se nenhum
uint16_t alarm_origin(void);                   // node_id de quem disparou (o próprio = local)
bool     alarm_siren_on(void);
