#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "aguada_idf";

void app_main(void) {
    ESP_LOGI(TAG, "ESP-IDF stub app running (this repo mainly uses PlatformIO).\n");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
