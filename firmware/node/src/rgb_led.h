#pragma once
#include <stdint.h>

typedef enum : uint8_t {
    RGB_OFF        = 0,
    RGB_NORMAL     = 1,   // green pulse  — nominal operation
    RGB_NO_GW      = 2,   // yellow pulse — no gateway heard
    RGB_SENSOR_ERR = 3,   // red pulse    — sensor read failed
    RGB_OTA        = 4,   // blue blink   — OTA in progress
} rgb_led_mode_t;

// Initialise WS2812B on the given GPIO. Call once in setup().
// Pass 0 to disable RGB LED (falls back to nothing).
void rgb_led_init(uint8_t pin);

// Set raw colour immediately (r/g/b 0-255).
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b);

// Apply a named status mode.
// NORMAL / NO_GW / SENSOR_ERR: emits a short colour pulse then turns off.
// OTA: stays on blue continuously.
void rgb_led_status(rgb_led_mode_t mode);
