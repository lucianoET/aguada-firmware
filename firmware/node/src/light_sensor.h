#pragma once
#include <stdint.h>
#include <stdbool.h>

// TEMT6000 — ambient light sensor (analog). Outputs a voltage proportional to
// illuminance through a 10 kΩ load on the breakout. Read on an ADC1 pin
// (GPIO0–4 on the ESP32-C3; ADC2/GPIO5 is unusable while WiFi is active).
typedef struct {
    uint16_t raw;    // raw ADC reading (0–4095, 12-bit)
    uint16_t mv;     // calibrated millivolts
    uint16_t lux;    // estimated illuminance (lux) — approximate, uncalibrated
    uint8_t  pct;    // relative brightness 0–100 % (mv / full-scale)
    bool     valid;
} LightReading;

// Configure the ADC pin (12-bit, full-range attenuation).
void light_init(uint8_t adc_pin);

// Average a few samples and return raw/mv/lux/pct.
LightReading light_read(void);
