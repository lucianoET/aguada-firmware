#pragma once
#include <stdint.h>
#include <stdbool.h>

// Adaptive sensor states for HC-SR04
typedef enum {
    SENSOR_STATE_BOOT,       // Initial readings, fast sampling
    SENSOR_STATE_STABLE,     // No changes, slow sampling, heartbeat only
    SENSOR_STATE_TRACKING    // Change detected, fast sampling, send updates
} sensor_adaptive_state_t;

typedef struct {
    sensor_adaptive_state_t state;
    uint16_t stable_value_cm;      // Current stable reading
    uint16_t stable_count;         // Consecutive stable readings
    uint32_t last_measure_ms;      // When last measurement was taken
    uint32_t last_send_ms;         // When last data was sent
    uint32_t last_hb_ms;           // When last heartbeat was sent
    bool     initialized;          // Has established baseline
} sensor_adaptive_t;

// Initialize adaptive sensor
void sensor_adaptive_init(sensor_adaptive_t *s);

// Process new reading, returns true if should send data
// out_distance: filtered value to send (if return is true)
// out_send_heartbeat: true if should send heartbeat instead
bool sensor_adaptive_update(sensor_adaptive_t *s, uint16_t filtered_cm,
                           uint16_t *out_distance, bool *out_send_heartbeat);

// Get current sampling interval in seconds
uint16_t sensor_adaptive_get_interval(sensor_adaptive_t *s);
