#include "mesh.h"
#include <string.h>
#include <esp_log.h>

static const char *TAG = "mesh";

static neighbor_t s_neighbors[MESH_MAX_NEIGHBORS];

void mesh_init(void) {
    memset(s_neighbors, 0, sizeof(s_neighbors));
}

void mesh_update_neighbor(uint16_t node_id, const uint8_t *mac, int8_t rssi, uint8_t hops_to_gw) {
    // Look for existing entry or empty slot
    int empty_slot = -1;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    
    for (int i = 0; i < MESH_MAX_NEIGHBORS; i++) {
        if (s_neighbors[i].node_id == node_id) {
            s_neighbors[i].rssi       = rssi;
            s_neighbors[i].hops_to_gw = hops_to_gw;
            s_neighbors[i].last_seen  = now_ms;  // milliseconds
            if (mac) memcpy(s_neighbors[i].mac, mac, 6);
            return;
        }
        if (empty_slot < 0 && s_neighbors[i].node_id == 0) empty_slot = i;
    }

    if (empty_slot >= 0) {
        s_neighbors[empty_slot].node_id    = node_id;
        s_neighbors[empty_slot].rssi       = rssi;
        s_neighbors[empty_slot].hops_to_gw = hops_to_gw;
        s_neighbors[empty_slot].last_seen  = now_ms;  // milliseconds
        if (mac) memcpy(s_neighbors[empty_slot].mac, mac, 6);
        ESP_LOGI(TAG, "New neighbor: 0x%04X hops=%d rssi=%d", node_id, hops_to_gw, rssi);
    }
}

void mesh_expire_neighbors(uint32_t timeout_s) {
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    uint32_t timeout_ms = timeout_s * 1000;
    
    for (int i = 0; i < MESH_MAX_NEIGHBORS; i++) {
        if (s_neighbors[i].node_id != 0) {
            uint32_t elapsed = now_ms - s_neighbors[i].last_seen;  // overflow-safe
            if (elapsed > timeout_ms) {
                ESP_LOGI(TAG, "Expired neighbor: 0x%04X (idle %u ms)", 
                         s_neighbors[i].node_id, elapsed);
                memset(&s_neighbors[i], 0, sizeof(neighbor_t));
            }
        }
    }
}

// Returns MAC of best relay toward gateway, or NULL if none
const uint8_t *mesh_best_relay(void) {
    int best_score  = -999;
    int best_idx    = -1;

    for (int i = 0; i < MESH_MAX_NEIGHBORS; i++) {
        if (s_neighbors[i].node_id == 0) continue;
        int score = (int)s_neighbors[i].rssi - ((int)s_neighbors[i].hops_to_gw * 10);
        if (score > best_score) {
            best_score = score;
            best_idx   = i;
        }
    }

    return (best_idx >= 0) ? s_neighbors[best_idx].mac : nullptr;
}

neighbor_t *mesh_get_neighbors(void) {
    return s_neighbors;
}
