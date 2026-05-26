#pragma once
#include <stdint.h>
#include <esp_timer.h>

#define MESH_MAX_NEIGHBORS 16

typedef struct {
    uint16_t node_id;
    uint8_t  mac[6];
    int8_t   rssi;
    uint8_t  hops_to_gw;
    uint32_t last_seen;     // millis
} neighbor_t;

void            mesh_init(void);
void            mesh_update_neighbor(uint16_t node_id, const uint8_t *mac, int8_t rssi, uint8_t hops_to_gw);
void            mesh_expire_neighbors(uint32_t timeout_s);
const uint8_t  *mesh_best_relay(void);
neighbor_t     *mesh_get_neighbors(void);
