#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "wmews";

void app_main(void)
{
    while (true) {
        ESP_LOGI(TAG, "Hello from the WMEWS ESP32-S3 firmware!");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
