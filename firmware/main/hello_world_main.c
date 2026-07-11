#include <time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"

#include "network.h"

static const char *TAG = "wmews";

#define SNTP_SYNC_RETRIES 5
#define SNTP_SYNC_WAIT_MS 2000

void app_main(void)
{
    ESP_LOGD(TAG, "Initializing NVS");
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(err));
        return;
    }

    if (initialize_wifi() != NETWORK_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization failed");
        return;
    }

    ESP_LOGD(TAG, "Initializing SNTP with pool.ntp.org");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP initialization failed: %s", esp_err_to_name(err));
        return;
    }

    for (int attempt = 1; attempt <= SNTP_SYNC_RETRIES; ++attempt) {
        err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(SNTP_SYNC_WAIT_MS));
        if (err == ESP_OK) {
            break;
        }
        if (attempt < SNTP_SYNC_RETRIES) {
            ESP_LOGW(TAG, "Waiting for SNTP synchronization (%d/%d)", attempt, SNTP_SYNC_RETRIES);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP synchronization failed: %s", esp_err_to_name(err));
        return;
    }

    time_t now;
    struct tm utc_time;
    char timestamp[sizeof("YYYY-MM-DDTHH:MM:SSZ")];
    time(&now);
    if (gmtime_r(&now, &utc_time) == NULL || strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc_time) == 0) {
        ESP_LOGE(TAG, "Could not format synchronized UTC time");
        return;
    }
    ESP_LOGI(TAG, "Synchronized UTC time: %s", timestamp);
}
