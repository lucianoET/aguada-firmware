/*
 * Ultrasonic Experiments - Implementações
 */

#include "ultrasonic_experiments.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <Arduino.h>
#include <NewPing.h>

static const char *TAG = "ultra_exp";

// ────────────────────────────────────────────────────────────────────────────
// Função Principal: us_dist_cm (mediana + percentual)
// ────────────────────────────────────────────────────────────────────────────
UltrasonicReading us_dist_cm(uint8_t trig_pin, uint8_t echo_pin, uint8_t samples,
                             uint16_t min_distance, uint16_t max_distance) {
    UltrasonicReading result = {0, 0, false};

    if (samples == 0 || samples > 10) {
        samples = 5;
    }

    if (max_distance <= min_distance) {
        ESP_LOGW(TAG, "us_dist_cm: intervalo inválido min=%u max=%u", min_distance, max_distance);
        return result;
    }

    NewPing sonar(trig_pin, echo_pin, max_distance);

    // ping_median retorna tempo em microsegundos (uS)
    unsigned long echo_us = sonar.ping_median(samples, max_distance);
    uint16_t distance_cm = (uint16_t)NewPing::convert_cm((unsigned int)echo_us);

    if (distance_cm < 2 || distance_cm > max_distance) {
        ESP_LOGW(TAG, "us_dist_cm: leitura inválida dist=%u cm", distance_cm);
        return result;
    }

    int32_t range = (int32_t)max_distance - (int32_t)min_distance;
    int32_t offset_dist = (int32_t)distance_cm - (int32_t)min_distance;

    uint8_t percent = 0;
    if (offset_dist <= 0) {
        percent = 0;
    } else if (offset_dist >= range) {
        percent = 100;
    } else {
        percent = (uint8_t)((offset_dist * 100) / range);
    }

    result.distance_cm = distance_cm;
    result.percent = percent;
    result.valid = true;

    ESP_LOGD(TAG, "us_dist_cm: %u cm, %u%% (NewPing median %u samples)",
             distance_cm, percent, samples);

    return result;
}

// ────────────────────────────────────────────────────────────────────────────
// Experimento 1: Implementação manual básica
// ────────────────────────────────────────────────────────────────────────────
uint16_t ultrameasure1(uint8_t echo, uint8_t trig, uint16_t divider) {
    const uint32_t TIMEOUT_US = 25000;  // ~400cm máximo
    
    // Enviar pulso de trigger (10µs)
    gpio_set_level((gpio_num_t)trig, 0);
    delayMicroseconds(2);
    gpio_set_level((gpio_num_t)trig, 1);
    delayMicroseconds(10);
    gpio_set_level((gpio_num_t)trig, 0);
    
    // Aguardar echo subir
    int64_t t_start = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)echo) == 0) {
        if ((esp_timer_get_time() - t_start) > TIMEOUT_US) {
            ESP_LOGD(TAG, "ultrameasure1: timeout aguardando echo HIGH");
            return 0;  // timeout
        }
    }
    
    // Medir duração do pulso echo
    int64_t t_echo_start = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)echo) == 1) {
        if ((esp_timer_get_time() - t_echo_start) > TIMEOUT_US) {
            ESP_LOGD(TAG, "ultrameasure1: timeout aguardando echo LOW");
            return 0;  // timeout
        }
    }
    int64_t t_echo_end = esp_timer_get_time();
    
    // Calcular distância
    uint32_t duration_us = (uint32_t)(t_echo_end - t_echo_start);
    uint16_t distance_cm = duration_us / divider;
    
    // Validar alcance (0-400cm)
    if (distance_cm > 400) {
        ESP_LOGD(TAG, "ultrameasure1: fora de alcance (%d cm)", distance_cm);
        return 0;
    }
    
    ESP_LOGI(TAG, "ultrameasure1: %d cm (duration=%lu µs)", distance_cm, duration_us);
    return distance_cm;
}

// ────────────────────────────────────────────────────────────────────────────
// Experimento 2: Usando biblioteca NewPing
// ────────────────────────────────────────────────────────────────────────────
uint16_t ultrameasure_newping(uint8_t echo, uint8_t trig, uint16_t max_cm) {
    NewPing sonar(trig, echo, max_cm);
    uint16_t distance_cm = (uint16_t)sonar.ping_cm(max_cm);
    if (distance_cm < 2 || distance_cm > max_cm) {
        return 0;
    }
    ESP_LOGI(TAG, "ultrameasure_newping: %u cm", distance_cm);
    return distance_cm;
}

// ────────────────────────────────────────────────────────────────────────────
// Experimento 3: Medição com múltiplas amostras (median filter)
// ────────────────────────────────────────────────────────────────────────────
uint16_t ultrameasure_median(uint8_t echo, uint8_t trig, uint8_t samples) {
    if (samples > 7) samples = 7;
    if (samples < 3) samples = 3;
    if ((samples % 2) == 0) samples++;  // Forçar ímpar para mediana
    
    uint16_t readings[7];
    uint8_t valid_count = 0;
    
    // Coletar amostras
    for (uint8_t i = 0; i < samples; i++) {
        uint16_t dist = ultrameasure1(echo, trig, 58);
        if (dist > 0) {
            readings[valid_count++] = dist;
        }
        delay(60);  // 60ms entre medições (HC-SR04 mínimo)
    }
    
    if (valid_count == 0) {
        ESP_LOGW(TAG, "ultrameasure_median: nenhuma leitura válida");
        return 0;
    }
    
    // Ordenar (bubble sort simples)
    for (uint8_t i = 0; i < valid_count - 1; i++) {
        for (uint8_t j = 0; j < valid_count - i - 1; j++) {
            if (readings[j] > readings[j + 1]) {
                uint16_t temp = readings[j];
                readings[j] = readings[j + 1];
                readings[j + 1] = temp;
            }
        }
    }
    
    // Retornar mediana
    uint16_t median = readings[valid_count / 2];
    ESP_LOGI(TAG, "ultrameasure_median: %d cm (de %d amostras válidas)", median, valid_count);
    return median;
}
