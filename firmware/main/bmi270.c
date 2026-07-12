#include "bmi270.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bmi270";

struct bmi270_handle {
    bmi270_config_t config;
    bool any_motion_enabled;
    uint32_t sensor_time;
    uint32_t random_state;
};

static bool is_valid_config(const bmi270_config_t *config)
{
    return config != NULL && config->sample_rate_hz >= BMI270_MIN_SAMPLE_RATE_HZ &&
           config->sample_rate_hz <= BMI270_MAX_SAMPLE_RATE_HZ;
}

static uint32_t sample_period_ms(const bmi270_handle_t *handle)
{
    return (1000U + handle->config.sample_rate_hz - 1U) / handle->config.sample_rate_hz;
}

static TickType_t sample_delay_ticks(const bmi270_handle_t *handle)
{
    TickType_t ticks = pdMS_TO_TICKS(sample_period_ms(handle));
    return ticks == 0 ? 1 : ticks;
}

static int16_t random_noise(bmi270_handle_t *handle, int16_t magnitude)
{
    handle->random_state = handle->random_state * UINT32_C(1664525) + UINT32_C(1013904223);
    uint32_t range = (uint32_t)magnitude * 2U + 1U;
    return (int16_t)(handle->random_state % range) - magnitude;
}

static int16_t sample_value(bmi270_handle_t *handle, int16_t center, int16_t noise)
{
    return (int16_t)(center + random_noise(handle, noise));
}

bmi270_error_t bmi270_open(const bmi270_config_t *config, bmi270_handle_t **handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Open failed: handle output is null");
        return BMI270_ERR_INVALID_ARGUMENT;
    }
    *handle = NULL;

    if (!is_valid_config(config)) {
        ESP_LOGE(TAG, "Open failed: invalid configuration");
        return BMI270_ERR_INVALID_CONFIG;
    }

    bmi270_handle_t *new_handle = calloc(1, sizeof(*new_handle));
    if (new_handle == NULL) {
        ESP_LOGE(TAG, "Open failed: could not allocate handle");
        return BMI270_ERR_NO_MEMORY;
    }

    new_handle->config = *config;
    new_handle->random_state = UINT32_C(0x27027027) ^ config->sample_rate_hz;
    *handle = new_handle;
    ESP_LOGI(TAG, "Open dummy backend: rate=%" PRIu32 "Hz", config->sample_rate_hz);
    return BMI270_OK;
}

bmi270_error_t bmi270_read(bmi270_handle_t *handle, bmi270_data_t *samples, size_t capacity,
                           size_t *samples_read, uint32_t timeout_ms)
{
    if (samples_read == NULL) {
        ESP_LOGE(TAG, "Read failed: samples_read is null");
        return BMI270_ERR_INVALID_ARGUMENT;
    }
    *samples_read = 0;

    if (handle == NULL) {
        ESP_LOGE(TAG, "Read failed: handle is null");
        return BMI270_ERR_INVALID_STATE;
    }
    if (capacity != 0 && samples == NULL) {
        ESP_LOGE(TAG, "Read failed: samples buffer is null for nonzero capacity");
        return BMI270_ERR_INVALID_ARGUMENT;
    }

    ESP_LOGI(TAG, "Read dummy FIFO: capacity=%u timeout=%" PRIu32 "ms", (unsigned)capacity, timeout_ms);
    if (capacity == 0) {
        return BMI270_OK;
    }

    uint32_t interval_ms = sample_period_ms(handle);
    if (timeout_ms != 0 && timeout_ms < interval_ms) {
        ESP_LOGE(TAG, "Read timed out before dummy sample interval (%" PRIu32 "ms)", interval_ms);
        return BMI270_ERR_TIMEOUT;
    }

    vTaskDelay(sample_delay_ticks(handle));
    uint32_t sensor_time_increment = UINT32_C(1000000) / handle->config.sample_rate_hz;
    if (sensor_time_increment == 0) {
        sensor_time_increment = 1;
    }

    for (size_t index = 0; index < capacity; ++index) {
        handle->sensor_time += sensor_time_increment;
        samples[index] = (bmi270_data_t){
            .sensor_time = handle->sensor_time,
            .accel_x = sample_value(handle, 0, 30),
            .accel_y = sample_value(handle, 0, 30),
            .accel_z = sample_value(handle, 1000, 30),
            .gyro_x = sample_value(handle, 0, 5),
            .gyro_y = sample_value(handle, 0, 5),
            .gyro_z = sample_value(handle, 0, 5),
            .temperature = sample_value(handle, 2500, 10),
            .flags = BMI270_DATA_ACCEL_VALID | BMI270_DATA_GYRO_VALID | BMI270_DATA_TEMPERATURE_VALID,
        };
    }

    *samples_read = capacity;
    return BMI270_OK;
}

bmi270_error_t bmi270_enable_anymotion_detection(bmi270_handle_t *handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Enable any-motion failed: handle is null");
        return BMI270_ERR_INVALID_STATE;
    }

    handle->any_motion_enabled = true;
    ESP_LOGI(TAG, "Enabled dummy any-motion detection");
    return BMI270_OK;
}

bmi270_error_t bmi270_disable_anymotion_detection(bmi270_handle_t *handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Disable any-motion failed: handle is null");
        return BMI270_ERR_INVALID_STATE;
    }

    handle->any_motion_enabled = false;
    ESP_LOGI(TAG, "Disabled dummy any-motion detection");
    return BMI270_OK;
}

bmi270_error_t bmi270_close(bmi270_handle_t *handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "Close failed: handle is null");
        return BMI270_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Close dummy backend");
    memset(handle, 0, sizeof(*handle));
    free(handle);
    return BMI270_OK;
}
