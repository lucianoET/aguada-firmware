#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "node_config.h"

typedef struct {
    uint16_t buf[DEFAULT_FILTER_WINDOW];
    uint8_t  buf_idx;
    uint8_t  window;
    uint8_t  outlier_cm;
    uint16_t moving_avg;
    bool     initialized;    // Auto-reset para mudanças reais grandes
    uint8_t  consecutive_rejects;
    uint16_t reject_value;} sensor_filter_t;

void     filter_init(sensor_filter_t *f, uint8_t window, uint8_t outlier_cm);
uint16_t filter_update(sensor_filter_t *f, uint16_t raw);
