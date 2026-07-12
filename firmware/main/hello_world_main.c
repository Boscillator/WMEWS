#include <time.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"

#include "bmi270.h"
#include "data_recorder.h"
#include "network.h"
#include "uploader.h"

static const char *TAG = "wmews";
static bmi270_handle_t *s_bmi270;

#define SNTP_SYNC_RETRIES 5
#define SNTP_SYNC_WAIT_MS 5000

static esp_err_t initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_ERR_NVS_NO_FREE_PAGES && err != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        return err;
    }

    ESP_LOGW(TAG, "NVS is full or incompatible; erasing it before retrying initialization");
    err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGW(TAG, "NVS erased; device credentials must be reprovisioned");
    return nvs_flash_init();
}

void app_main(void)
{
    ESP_LOGD(TAG, "Initializing NVS");
    esp_err_t err = initialize_nvs();
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
            ESP_LOGW(TAG, "Waiting for SNTP synchronization (%d/%d). Failed: %s", attempt, SNTP_SYNC_RETRIES, esp_err_to_name(err));
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

    const bmi270_config_t sensor_config = BMI270_DEFAULT_CONFIG();
    const bmi270_error_t sensor_result = bmi270_open(&sensor_config, &s_bmi270);
    if (sensor_result != BMI270_OK) {
        ESP_LOGE(TAG, "BMI270 initialization failed: %d", sensor_result);
        return;
    }

    uploader_handoff_t handoff;
    data_recorder_error_t recorder_result = data_recorder_initialize(&handoff);
    if (recorder_result != DATA_RECORDER_OK) {
        ESP_LOGE(TAG, "Recorder initialization failed: %d", recorder_result);
        (void)bmi270_close(s_bmi270);
        s_bmi270 = NULL;
        return;
    }

    uploader_context_t *uploader;
    uploader_error_t uploader_result = uploader_initialize(&handoff, &uploader);
    if (uploader_result != UPLOADER_OK) {
        ESP_LOGE(TAG, "Uploader initialization failed: %d", uploader_result);
        (void)bmi270_close(s_bmi270);
        s_bmi270 = NULL;
        return;
    }
    uploader_result = uploader_start(uploader);
    if (uploader_result != UPLOADER_OK) {
        ESP_LOGE(TAG, "Uploader startup failed: %d", uploader_result);
        (void)bmi270_close(s_bmi270);
        s_bmi270 = NULL;
        return;
    }

    recorder_result = data_recorder_start(s_bmi270, &handoff);
    if (recorder_result != DATA_RECORDER_OK) {
        ESP_LOGE(TAG, "Recorder startup failed: %d", recorder_result);
        (void)bmi270_close(s_bmi270);
        s_bmi270 = NULL;
        return;
    }
}
