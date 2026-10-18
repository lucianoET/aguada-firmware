#pragma once
#include <stdint.h>
#include <stdbool.h>

// INMP441 — microfone MEMS digital I2S (substitui o KY-037/038 analógico).
// Ligação: VDD 3V3, GND, L/R → GND (canal esquerdo), SCK/WS/SD nos GPIO abaixo.
// Sensibilidade de fábrica -26 dBFS @ 94 dB SPL (±1 dB) → nível absoluto sem calibrar.
// Nível em **dB(A)** (ponderação A, IEC 61672, filtro IIR a 32 kHz: ±1 dB de 25 Hz a
// 6,3 kHz). A ponderação A é o padrão para ruído ambiente e corta a deriva de baixa
// frequência do MEMS (< 50 Hz) e o zumbido de 60/120 Hz, que inflavam a leitura Z.
// Não é sonómetro classe 1/2 (janela curta, sem Leq contínuo); comparar com um
// sonómetro/app e acertar MIC_DB_OFFSET se precisar.
//
// Detecção: SD tem pull-down; sem microfone o I2S lê zeros → valid=false.
typedef struct {
    float db_spl;    // nível RMS ponderado A de uma janela de 0,26 s por tick, dB(A)
    float peak_db;   // pico instantâneo ponderado A da janela, dB(A)
    float db_raw;    // nível sem o offset de calibração de campo (calib().db)
    bool  valid;
} SoundReading;

void sound_init(int8_t sck_pin, int8_t ws_pin, int8_t sd_pin);   // -1 = desactivado

// Lê uma janela de áudio. Bloqueia ~0,4 s (descarte do buffer DMA + janela + filtro).
SoundReading sound_read(void);
