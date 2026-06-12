#include "sensor_adaptive.h"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_timer.h>

static const char *TAG = "adaptive";

// Timing configuration (seconds)
#define INTERVAL_BOOT       5      // Fast sampling during boot
#define INTERVAL_TRACKING   10     // Moderate sampling when tracking changes
#define INTERVAL_STABLE     60     // Slow sampling when stable
#define HEARTBEAT_INTERVAL  60     // Heartbeat every 60s in stable mode

// Thresholds (cm)
#define STABLE_THRESHOLD    2      // Within ±2cm = stable
#define CHANGE_THRESHOLD    3      // >3cm = significant change detected
#define SEND_THRESHOLD      1      // Send if changed by >1cm
#define STABLE_COUNT_NEEDED 5      // 5 consecutive stable readings to enter STABLE

static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void sensor_adaptive_init(sensor_adaptive_t *s) {
    s->state = SENSOR_STATE_BOOT;
    s->stable_value_cm = 0;
    s->stable_count = 0;
    s->last_measure_ms = 0;
    s->last_send_ms = 0;
    s->last_hb_ms = 0;
    s->initialized = false;
    ESP_LOGI(TAG, "State: BOOT (fast sampling)");
}

bool sensor_adaptive_update(sensor_adaptive_t *s, uint16_t filtered_cm,
                           uint16_t *out_distance, bool *out_send_heartbeat) {
    uint32_t now = now_ms();
    *out_send_heartbeat = false;
    
    // Check if it's time to measure
    uint16_t interval_ms = sensor_adaptive_get_interval(s) * 1000;
    if ((now - s->last_measure_ms) < interval_ms) {
        return false;  // Not time yet
    }
    s->last_measure_ms = now;

    // Calculate delta from stable value
    int delta = (int)filtered_cm - (int)s->stable_value_cm;
    if (delta < 0) delta = -delta;

    switch (s->state) {
        case SENSOR_STATE_BOOT:
            // During boot, establish baseline quickly
            if (!s->initialized) {
                s->stable_value_cm = filtered_cm;
                s->stable_count = 1;
                s->initialized = true;
                *out_distance = filtered_cm;
                s->last_send_ms = now;
                s->last_hb_ms = now;
                ESP_LOGI(TAG, "Baseline: %dcm", filtered_cm);
                return true;  // Send first reading immediately
            }

            // Check if value is stable
            if (delta <= STABLE_THRESHOLD) {
                s->stable_count++;
                ESP_LOGD(TAG, "BOOT stable count: %d/%d", s->stable_count, STABLE_COUNT_NEEDED);
                
                if (s->stable_count >= STABLE_COUNT_NEEDED) {
                    // Transition to STABLE
                    s->state = SENSOR_STATE_STABLE;
                    s->stable_value_cm = filtered_cm;
                    s->stable_count = 0;
                    ESP_LOGI(TAG, "State: BOOT → STABLE (value=%dcm)", filtered_cm);
                    // Send final boot value
                    *out_distance = filtered_cm;
                    s->last_send_ms = now;
                    s->last_hb_ms = now;
                    return true;
                }
            } else {
                // Value changed during boot
                s->stable_value_cm = filtered_cm;
                s->stable_count = 1;
                
                // Send if changed by threshold
                if (delta > SEND_THRESHOLD) {
                    *out_distance = filtered_cm;
                    s->last_send_ms = now;
                    ESP_LOGI(TAG, "BOOT update: %dcm (delta=%d)", filtered_cm, delta);
                    return true;
                }
            }
            break;

        case SENSOR_STATE_STABLE:
            // In stable state, only send heartbeats unless significant change
            if (delta > CHANGE_THRESHOLD) {
                // Significant change detected → transition to TRACKING
                s->state = SENSOR_STATE_TRACKING;
                s->stable_value_cm = filtered_cm;
                s->stable_count = 0;
                s->last_measure_ms = now - (INTERVAL_TRACKING * 1000);  // Measure immediately
                ESP_LOGI(TAG, "State: STABLE → TRACKING (change=%dcm)", delta);
                *out_distance = filtered_cm;
                s->last_send_ms = now;
                s->last_hb_ms = now;
                return true;
            }

            // Check if it's time for heartbeat
            if ((now - s->last_hb_ms) >= (HEARTBEAT_INTERVAL * 1000)) {
                s->last_hb_ms = now;
                *out_send_heartbeat = true;
                ESP_LOGD(TAG, "STABLE heartbeat (value=%dcm)", s->stable_value_cm);
                return true;
            }
            break;

        case SENSOR_STATE_TRACKING:
            // Tracking changes, send updates frequently
            if (delta <= STABLE_THRESHOLD) {
                s->stable_count++;
                ESP_LOGD(TAG, "TRACKING stable count: %d/%d", s->stable_count, STABLE_COUNT_NEEDED);
                
                if (s->stable_count >= STABLE_COUNT_NEEDED) {
                    // Transition back to STABLE
                    s->state = SENSOR_STATE_STABLE;
                    s->stable_value_cm = filtered_cm;
                    s->stable_count = 0;
                    ESP_LOGI(TAG, "State: TRACKING → STABLE (stabilized at %dcm)", filtered_cm);
                    *out_distance = filtered_cm;
                    s->last_send_ms = now;
                    s->last_hb_ms = now;
                    return true;
                }
            } else {
                // Value still changing
                s->stable_count = 0;
                
                // Update stable value and send if threshold exceeded
                int send_delta = (int)filtered_cm - (int)s->stable_value_cm;
                if (send_delta < 0) send_delta = -send_delta;
                
                if (send_delta > SEND_THRESHOLD) {
                    s->stable_value_cm = filtered_cm;
                    *out_distance = filtered_cm;
                    s->last_send_ms = now;
                    ESP_LOGI(TAG, "TRACKING update: %dcm (delta=%d)", filtered_cm, send_delta);
                    return true;
                }
            }
            break;
    }

    return false;
}

uint16_t sensor_adaptive_get_interval(sensor_adaptive_t *s) {
    switch (s->state) {
        case SENSOR_STATE_BOOT:     return INTERVAL_BOOT;
        case SENSOR_STATE_TRACKING: return INTERVAL_TRACKING;
        case SENSOR_STATE_STABLE:   return INTERVAL_STABLE;
        default:                    return INTERVAL_STABLE;
    }
}
