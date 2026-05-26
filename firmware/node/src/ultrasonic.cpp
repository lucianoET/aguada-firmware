#include "ultrasonic.h"
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "hcsr04";

#define TRIG_PULSE_US   10
#define ECHO_TIMEOUT_US 25000   // ~4m max range
#define SOUND_CM_US     58      // 1cm ≈ 58µs round trip

void ultrasonic_init(uint8_t trig_pin, uint8_t echo_pin) {
    gpio_config_t io = {};

    io.pin_bit_mask = (1ULL << trig_pin);
    io.mode         = GPIO_MODE_OUTPUT;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.pull_up_en   = GPIO_PULLUP_DISABLE;
    io.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&io);
    gpio_set_level((gpio_num_t)trig_pin, 0);

    io.pin_bit_mask = (1ULL << echo_pin);
    io.mode         = GPIO_MODE_INPUT;
    gpio_config(&io);
}

uint16_t ultrasonic_read_cm(uint8_t trig_pin, uint8_t echo_pin) {
    // HC-SR04 stability delay - critical for reliable readings
    esp_rom_delay_us(100000);  // 100ms stabilization time
    
    // Ensure echo pin is LOW before starting (clear any residual state)
    int64_t t_clear = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)echo_pin)) {
        if (esp_timer_get_time() - t_clear > 50000) {  // 50ms max wait
            ESP_LOGD(TAG, "echo stuck HIGH, aborting");
            return 0;
        }
        esp_rom_delay_us(1000);  // 1ms delay
    }
    
    // Send 10µs trigger pulse
    gpio_set_level((gpio_num_t)trig_pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level((gpio_num_t)trig_pin, 1);
    esp_rom_delay_us(TRIG_PULSE_US);
    gpio_set_level((gpio_num_t)trig_pin, 0);

    // Wait for echo HIGH
    int64_t t0 = esp_timer_get_time();
    while (!gpio_get_level((gpio_num_t)echo_pin)) {
        if (esp_timer_get_time() - t0 > ECHO_TIMEOUT_US) {
            ESP_LOGD(TAG, "echo start timeout");
            return 0;
        }
    }

    // Measure echo pulse width
    int64_t t1 = esp_timer_get_time();
    while (gpio_get_level((gpio_num_t)echo_pin)) {
        if (esp_timer_get_time() - t1 > ECHO_TIMEOUT_US) {
            ESP_LOGD(TAG, "echo end timeout");
            return 0;
        }
    }

    int64_t duration_us = esp_timer_get_time() - t1;
    uint16_t dist = (uint16_t)(duration_us / SOUND_CM_US);

    if (dist == 0 || dist > HC_MAX_RANGE_CM) return 0;
    return dist;
}
