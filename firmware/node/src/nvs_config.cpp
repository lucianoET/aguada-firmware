#include "nvs_config.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_log.h>
#include <string.h>

static const char *TAG = "nvs_cfg";

#ifndef DEFAULT_NUM_SENSORS
#define DEFAULT_NUM_SENSORS 1
#endif

void nvs_config_defaults(node_config_t *cfg, uint16_t node_id) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->node_id              = node_id;
    
    // Build-time override for relay images
    if (DEFAULT_NUM_SENSORS == 0) {
        cfg->num_sensors = 0;
        cfg->sensor[1].trig_pin   = DEFAULT_TRIG2;
        cfg->sensor[1].echo_pin   = DEFAULT_ECHO2;
        cfg->sensor[1].enabled    = false;
    }
    // Build-time override for 2-sensor images
    else if (DEFAULT_NUM_SENSORS == 2) {
        cfg->num_sensors = 2;
        cfg->sensor[1].trig_pin   = DEFAULT_TRIG2;
        cfg->sensor[1].echo_pin   = DEFAULT_ECHO2;
        cfg->sensor[1].enabled    = true;
    }
    // Legacy: 2 sensors by node_id (CIE=0x9EAC, CB3=0x2EC4)
    else if (node_id == 0x9EAC || node_id == 0x2EC4) {
        cfg->num_sensors = 2;
        cfg->sensor[1].trig_pin   = DEFAULT_TRIG2;
        cfg->sensor[1].echo_pin   = DEFAULT_ECHO2;
        cfg->sensor[1].enabled    = true;
    } else {
        cfg->num_sensors = 1;
        cfg->sensor[1].trig_pin   = DEFAULT_TRIG2;
        cfg->sensor[1].echo_pin   = DEFAULT_ECHO2;
        cfg->sensor[1].enabled    = false;
    }

    cfg->sensor[0].trig_pin   = DEFAULT_TRIG1;
    cfg->sensor[0].echo_pin   = DEFAULT_ECHO1;
    cfg->sensor[0].enabled    = true;

    cfg->interval_measure_s   = DEFAULT_MEASURE_S;
    cfg->interval_send_s      = DEFAULT_SEND_S;
    cfg->heartbeat_s          = DEFAULT_HEARTBEAT_S;

    cfg->filter_window        = DEFAULT_FILTER_WINDOW;
    cfg->filter_outlier_cm    = DEFAULT_OUTLIER_CM;
    cfg->filter_threshold_cm  = DEFAULT_THRESHOLD_CM;

    cfg->vbat_enabled         = false;
    cfg->vbat_pin             = 0;
    cfg->vbat_div             = 2;   // default: equal-resistor voltage divider
    cfg->deep_sleep_enabled   = false;
    cfg->led_enabled          = true;

    cfg->espnow_channel       = ESPNOW_CHANNEL;
    cfg->ttl_max              = DEFAULT_TTL;
    cfg->restart_daily_h      = 0;

    strncpy(cfg->fw_version, FW_VERSION, sizeof(cfg->fw_version) - 1);
}

esp_err_t nvs_config_load(node_config_t *cfg, uint16_t node_id) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t h;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No NVS config found, using defaults");
        nvs_config_defaults(cfg, node_id);
        return ESP_ERR_NOT_FOUND;
    }

    size_t sz = sizeof(node_config_t);
    err = nvs_get_blob(h, "config", cfg, &sz);
    nvs_close(h);

    if (err != ESP_OK || sz != sizeof(node_config_t)) {
        ESP_LOGW(TAG, "NVS config invalid, using defaults");
        nvs_config_defaults(cfg, node_id);
        return ESP_ERR_NOT_FOUND;
    }

    // Always refresh node_id and fw_version from runtime values
    cfg->node_id = node_id;
    strncpy(cfg->fw_version, FW_VERSION, sizeof(cfg->fw_version) - 1);

    ESP_LOGI(TAG, "Config loaded: num_sensors=%d, measure=%ds, send=%ds",
             cfg->num_sensors, cfg->interval_measure_s, cfg->interval_send_s);
    return ESP_OK;
}

esp_err_t nvs_config_save(const node_config_t *cfg) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_blob(h, "config", cfg, sizeof(node_config_t));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);

    if (err == ESP_OK)
        ESP_LOGI(TAG, "Config saved to NVS");
    else
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(err));

    return err;
}
