#pragma once
#include <stdint.h>
#include <stddef.h>

// Voz e alarme por UDP multicast 239.255.42.99:5004 (doc: COMMUNICATOR.md §Voz).
//   "AV": 10 B de cabeçalho + AUDIO_FRAME amostras µ-law (20 ms)
//   "AC": alarme (espelho do SENSOR_ID_ALARM do ESP-NOW, para quem só tem WiFi)

#define VOICE_FLAG_END   0x01            // último pacote do PTT

typedef void (*alarm_rx_cb_t)(uint16_t node_id, uint16_t seq, uint8_t code, uint8_t zone);

void voice_net_begin(uint16_t node_id, alarm_rx_cb_t on_alarm);   // depois do WiFi conectar
bool voice_net_ready(void);

// TX
void voice_send(const int16_t *pcm, size_t n, bool end);
void alarm_send_udp(uint16_t seq, uint8_t code, uint8_t zone);

// RX: buffer de jitter. voice_pop devolve true e enche `out` quando há áudio a tocar;
// false = nada (silêncio ou ainda a pré-bufferizar).
bool     voice_pop(int16_t *out, size_t n);
uint16_t voice_talker(void);             // node_id de quem está falando (0 = ninguém)
void     voice_rx_enable(bool on);       // desligado durante o próprio PTT
