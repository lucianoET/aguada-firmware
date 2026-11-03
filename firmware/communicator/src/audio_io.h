#pragma once
#include <stdint.h>
#include <stddef.h>

// I2S full-duplex num controlador só: INMP441 (RX, canal esquerdo; 2º mic opcional no direito) e MAX98357A/PCM5102A
// (TX, mesmo sinal nos dois canais) com BCLK/WS compartilhados. 16 kHz, slots de 32 bit.

bool audio_init(void);
bool audio_mic_present(void);          // detectado no boot (SD com pull-down → zeros)
bool audio_mic_stereo(void);           // 2º mic no canal R (L/R em 3V3): voz = média L+R
void audio_mic_energy(float *l, float *r);   // energia L/R do último audio_read_mic

// Lê n amostras mono do mic (24 bit úteis, alinhadas em int32). Bloqueia no ritmo do I2S.
size_t audio_read_mic(int32_t *out, size_t n);
void   audio_flush_mic(void);          // descarta o que o DMA acumulou (antes do PTT)

// Escreve n amostras mono de 16 bit com o volume aplicado. Bloqueia no ritmo do I2S.
void    audio_write(const int16_t *in, size_t n);
void    audio_set_volume(uint8_t pct); // 0..100
uint8_t audio_volume(void);
