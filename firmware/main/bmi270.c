#include "bmi270.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bmi270";

static const uint32_t DUMMY_SAMPLE_RATE_HZ = 100U;
static const uint32_t DUMMY_SENSOR_TIME_DT_US = 10000U;
static const float DUMMY_GYRO_LSB = 1.0F;
static const float DUMMY_ACCEL_LSB = 1.0F;
static const float DUMMY_TEMPERATURE_LSB = 1.0F;

struct bmi270_handle {
    bmi270_config_t config;
    uint32_t sample_rate_hz;
    uint32_t sensor_time_dt_us;
    float gyro_lsb;
    float accel_lsb;
    float temperature_lsb;
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

static void delay_for_samples(const bmi270_handle_t *handle, size_t sample_count)
{
    const TickType_t sample_ticks = sample_delay_ticks(handle);

    // Keep generated samples aligned with the configured sample rate, including batch reads.
    for (size_t index = 0; index < sample_count; ++index) {
        vTaskDelay(sample_ticks);
    }
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
    new_handle->sample_rate_hz = DUMMY_SAMPLE_RATE_HZ;
    new_handle->sensor_time_dt_us = DUMMY_SENSOR_TIME_DT_US;
    new_handle->gyro_lsb = DUMMY_GYRO_LSB;
    new_handle->accel_lsb = DUMMY_ACCEL_LSB;
    new_handle->temperature_lsb = DUMMY_TEMPERATURE_LSB;
    new_handle->random_state = UINT32_C(0x27027027) ^ config->sample_rate_hz;
    *handle = new_handle;
    ESP_LOGI(TAG, "Open dummy backend: rate=%" PRIu32 "Hz", config->sample_rate_hz);
    return BMI270_OK;
}

uint32_t bmi270_get_sample_rate_hz(const bmi270_handle_t *handle)
{
    return handle == NULL ? 0U : handle->sample_rate_hz;
}

uint32_t bmi270_get_sensor_time_dt_us(const bmi270_handle_t *handle)
{
    return handle == NULL ? 0U : handle->sensor_time_dt_us;
}

float bmi270_get_gyro_lsb(const bmi270_handle_t *handle)
{
    return handle == NULL ? 0.0F : handle->gyro_lsb;
}

float bmi270_get_accel_lsb(const bmi270_handle_t *handle)
{
    return handle == NULL ? 0.0F : handle->accel_lsb;
}

float bmi270_get_temperature_lsb(const bmi270_handle_t *handle)
{
    return handle == NULL ? 0.0F : handle->temperature_lsb;
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

    // ESP_LOGI(TAG, "Read dummy FIFO: capacity=%u timeout=%" PRIu32 "ms", (unsigned)capacity, timeout_ms);
    if (capacity == 0) {
        return BMI270_OK;
    }

    const uint32_t interval_ms = sample_period_ms(handle);
    const uint64_t required_wait_ms = (uint64_t)interval_ms * capacity;
    if (timeout_ms != 0U && (uint64_t)timeout_ms < required_wait_ms) {
        ESP_LOGE(TAG, "Read timed out before %u dummy samples (%" PRIu64 "ms)",
                 (unsigned)capacity, required_wait_ms);
        return BMI270_ERR_TIMEOUT;
    }

    delay_for_samples(handle, capacity);
    const uint32_t sensor_time_increment = handle->sensor_time_dt_us;

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
