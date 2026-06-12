#pragma once
#include <stdint.h>
#include "node_config.h"

void     ultrasonic_init(uint8_t trig_pin, uint8_t echo_pin);
uint16_t ultrasonic_read_cm(uint8_t trig_pin, uint8_t echo_pin);
