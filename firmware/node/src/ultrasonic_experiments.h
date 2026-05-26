/*
 * Ultrasonic helpers for Aguada nodes (v4.0).
 *
 * Main path uses NewPing median sampling and returns
 * distance + percentage normalized to reservoir range.
 */

#pragma once
#include <stdint.h>

// ────────────────────────────────────────────────────────────────────────────
// Estrutura de retorno da função us_dist_cm
// ────────────────────────────────────────────────────────────────────────────
struct UltrasonicReading {
    uint16_t distance_cm;  // Distância em cm
    uint8_t percent;       // Percentual 0-100
    bool valid;            // Reading válido
};

// ────────────────────────────────────────────────────────────────────────────
// Função principal: us_dist_cm (NewPing mediana + percentual)
// ────────────────────────────────────────────────────────────────────────────
// @param trig_pin: pino TRIG
// @param echo_pin: pino ECHO
// @param samples: número de amostras para mediana (1-10, recomendado 5)
// @param min_distance: distância mínima (sensor_offset em Aguada)
// @param max_distance: distância máxima (sensor_offset + level_max em Aguada)
// @return estrutura com distance_cm, percent e valid
UltrasonicReading us_dist_cm(uint8_t trig_pin, uint8_t echo_pin, uint8_t samples, 
                             uint16_t min_distance, uint16_t max_distance);

// ────────────────────────────────────────────────────────────────────────────
// Experimento 1: Implementação manual básica
// ────────────────────────────────────────────────────────────────────────────
// @param echo: pino ECHO
// @param trig: pino TRIG
// @param divider: divisor para conversão (58 para cm, 148 para polegadas)
// @return distância em cm (0 = erro/fora de alcance)
uint16_t ultrameasure1(uint8_t echo, uint8_t trig, uint16_t divider);

// ────────────────────────────────────────────────────────────────────────────
// Medição unitária com biblioteca NewPing
// ────────────────────────────────────────────────────────────────────────────
// @param echo: pino ECHO
// @param trig: pino TRIG
// @param max_cm: alcance máximo em cm (padrão 400)
// @return distância em cm (0 = erro/fora de alcance)
uint16_t ultrameasure_newping(uint8_t echo, uint8_t trig, uint16_t max_cm);

// ────────────────────────────────────────────────────────────────────────────
// Experimento 3: Medição com múltiplas amostras (median filter)
// ────────────────────────────────────────────────────────────────────────────
// @param echo: pino ECHO
// @param trig: pino TRIG
// @param samples: número de amostras (3, 5, ou 7)
// @return distância mediana em cm
uint16_t ultrameasure_median(uint8_t echo, uint8_t trig, uint8_t samples);
