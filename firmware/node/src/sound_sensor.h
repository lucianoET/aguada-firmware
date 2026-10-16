#pragma once
#include <stdint.h>
#include <stdbool.h>

// INMP441 — microfone MEMS digital I2S (substitui o KY-037/038 analógico).
// Ligação: VDD 3V3, GND, L/R → GND (canal esquerdo), SCK/WS/SD nos GPIO abaixo.
// Saída já digital e com sensibilidade de fábrica (-26 dBFS @ 94 dB SPL, ±1 dB),
// por isso o nível é dB SPL real (ponderação Z — sem filtro A; não é sonómetro
// classe 1/2, mas é comparável entre nodes sem calibração individual).
//
// Detecção: SD tem pull-down; sem microfone o I2S lê zeros → valid=false.
typedef struct {
    float db_spl;    // nível RMS de uma janela de 0.26 s por tick (amostra, não Leq contínuo), dB SPL (Z)
    float peak_db;   // pico da janela, dB SPL
    bool  valid;
} SoundReading;

void sound_init(int8_t sck_pin, int8_t ws_pin, int8_t sd_pin);   // -1 = desactivado

// Lê uma janela de áudio. Bloqueia ~0.35 s (descarte do buffer DMA + janela).
SoundReading sound_read(void);
