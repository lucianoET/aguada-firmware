#pragma once
#include <stdint.h>
#include <stdbool.h>

// Transformador de corrente de núcleo aberto (SCT-013-0xx, saída em TENSÃO com
// burden interno, ex. SCT-013-030 = 1 V RMS @ 30 A) no cabo de alimentação da
// condensadora. Dá: compressor ligado/desligado, ciclo de trabalho e consumo.
//
// Circuito na placa (porta CT, ADC1):
//   3V3 ─10k─┬─6k8─ GND   (bias ~1.34 V — NÃO usar meio da escala: o ADC do C3
//            │              com 11 dB só é linear até ~2.5 V)
//            ├─ 10 µF ─ GND
//            └─ anel (sleeve) do jack P2 do SCT-013;  ponta (tip) → GPIO do CT
//   Jack P2 COM contacto de comutação: sem plugue, o GPIO fica ligado ao GND.
//   → média ~0 V = "TC ausente"; média ~1.34 V = TC presente (detecção automática).
//
// Faixa linear: ±1.2 V de pico em torno do bias ≈ 0.85 V RMS → ~25 A com o
// SCT-013-030. Self-contained/centrais grandes: usar SCT-013-050/100 e ajustar
// CT_AMPS_PER_VOLT. Saturação é detectada (clipped=true) e registada no log.
//
// A medição bloqueia ~200 ms (12 ciclos de 60 Hz) a cada tick local.
#ifndef CT_AMPS_PER_VOLT
#define CT_AMPS_PER_VOLT 30.0f      // SCT-013-030: 30 A / 1 V
#endif

typedef struct {
    float    amps;      // corrente RMS (A)
    uint16_t bias_mv;   // média do sinal (diagnóstico do circuito de bias)
    bool     present;   // TC ligado (bias detectado)
    bool     clipped;   // sinal saiu da faixa linear do ADC — corrente subestimada
    bool     valid;
} CurrentReading;

void current_init(int8_t adc_pin);   // -1 = desactivado
CurrentReading current_read(void);
