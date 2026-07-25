#include "imu_pipeline.h"

#include <stdbool.h>

#include "esp_log.h"
#include "feature_pipeline.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "imu_pipeline";
enum { IMU_PIPELINE_TASK_STACK_SIZE = 4096U };

struct imu_pipeline_context {
    imu_buffer_pool_t pool;
    bool initialized;
    bool started;
};

static imu_pipeline_context_t s_context;

static void return_window_or_retry(const imu_pipeline_context_t *context, const imu_window_t *window)
{
    while (xQueueSend(context->pool.free_queue, window, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Could not return discarded window; retrying");
    }
}

static void imu_pipeline_task(void *argument)
{
    imu_pipeline_context_t *const context = argument;
    for (;;) {
        imu_window_t window;
        if (xQueueReceive(context->pool.pipeline_queue, &window, portMAX_DELAY) != pdPASS) continue;

        imu_feature_vector_t features;
        const feature_pipeline_error_t result = feature_pipeline_compute(&window, &features);
        if (result != FEATURE_PIPELINE_OK) {
            ESP_LOGE(TAG, "Feature computation failed: %d", result);
            return_window_or_retry(context, &window);
            continue;
        }
        ESP_LOGD(TAG, "Window features: samples=%u max_delta=%u", (unsigned)features.sample_count,
                 (unsigned)features.max_axis_delta_lsb);

        /* A local model consumes `features` here; raw-window ownership remains unchanged. */
        if (xQueueSend(context->pool.upload_queue, &window, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Could not transfer processed window to uploader");
            return_window_or_retry(context, &window);
        }
    }
}

imu_pipeline_error_t imu_pipeline_initialize(const imu_buffer_pool_t *pool,
                                             imu_pipeline_context_t **context)
{
    if (pool == NULL || context == NULL || pool->free_queue == NULL || pool->pipeline_queue == NULL ||
        pool->upload_queue == NULL) {
        ESP_LOGE(TAG, "Initialization requires a complete buffer pool and context output");
        return IMU_PIPELINE_ERR_INVALID_ARGUMENT;
    }
    if (s_context.initialized) {
        ESP_LOGE(TAG, "Pipeline is already initialized");
        return IMU_PIPELINE_ERR_INVALID_STATE;
    }
    s_context.pool = *pool;
    s_context.initialized = true;
    *context = &s_context;
    return IMU_PIPELINE_OK;
}

imu_pipeline_error_t imu_pipeline_start(imu_pipeline_context_t *context)
{
    if (context != &s_context || !context->initialized || context->started) {
        ESP_LOGE(TAG, "Start requested in invalid state");
        return IMU_PIPELINE_ERR_INVALID_STATE;
    }
    if (xTaskCreate(imu_pipeline_task, "imu_pipeline", IMU_PIPELINE_TASK_STACK_SIZE, context, 5U, NULL) !=
        pdPASS) {
        ESP_LOGE(TAG, "Could not create pipeline task");
        return IMU_PIPELINE_ERR_TASK_CREATE_FAILED;
    }
    context->started = true;
    ESP_LOGI(TAG, "IMU pipeline task started");
    return IMU_PIPELINE_OK;
}
