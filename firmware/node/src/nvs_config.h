#pragma once
#include "node_config.h"
#include <esp_err.h>

void     nvs_config_defaults(node_config_t *cfg, uint16_t node_id);
esp_err_t nvs_config_load(node_config_t *cfg, uint16_t node_id);
esp_err_t nvs_config_save(const node_config_t *cfg);
