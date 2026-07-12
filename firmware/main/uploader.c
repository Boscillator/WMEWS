#include "uploader.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "uploader";
static const TickType_t RETURN_RETRY_TICKS = pdMS_TO_TICKS(1000);

struct uploader_context {
    uploader_handoff_t handoff;
    bool initialized;
    bool started;
};

static uploader_context_t s_context;

static void return_window_or_retry(const uploader_context_t *context, const acceleration_window_t *window)
{
    while (xQueueSend(context->handoff.free_queue, window, RETURN_RETRY_TICKS) != pdPASS) {
        ESP_LOGE(TAG, "Fatal ownership invariant violation: could not return buffer; retrying");
    }
}

static void uploader_task(void *argument)
{
    const uploader_context_t *context = argument;
    acceleration_window_t window;

    for (;;) {
        if (xQueueReceive(context->handoff.ready_queue, &window, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (window.samples == NULL || window.count == 0U || window.count > window.capacity) {
            ESP_LOGE(TAG, "Received invalid window: samples=%p count=%u capacity=%u",
                     (void *)window.samples, (unsigned)window.count, (unsigned)window.capacity);
            return_window_or_retry(context, &window);
            continue;
        }

        uint64_t sum_of_squares = 0U;
        for (size_t index = 0; index < window.count; ++index) {
            const int64_t x = window.samples[index].accel_x;
            const int64_t y = window.samples[index].accel_y;
            const int64_t z = window.samples[index].accel_z;
            sum_of_squares += (uint64_t)(x * x + y * y + z * z);
        }

        const double rms_raw = sqrt((double)sum_of_squares / (double)window.count);
        const double rms_scaled = rms_raw * (double)window.accel_lsb;
        ESP_LOGI(TAG, "Window samples=%u RMS=%.3f raw, %.3f scaled", (unsigned)window.count,
                 rms_raw, rms_scaled);
        return_window_or_retry(context, &window);
    }
}

uploader_error_t uploader_initialize(const uploader_handoff_t *handoff, uploader_context_t **context)
{
    if (handoff == NULL || context == NULL || handoff->free_queue == NULL || handoff->ready_queue == NULL) {
        ESP_LOGE(TAG, "Initialize failed: invalid handoff or output context");
        return UPLOADER_ERR_INVALID_ARGUMENT;
    }
    if (s_context.initialized) {
        ESP_LOGE(TAG, "Initialize failed: uploader is already initialized");
        return UPLOADER_ERR_INVALID_STATE;
    }

    s_context.handoff = *handoff;
    s_context.initialized = true;
    *context = &s_context;
    return UPLOADER_OK;
}

uploader_error_t uploader_start(uploader_context_t *context)
{
    if (context != &s_context || !context->initialized || context->started) {
        ESP_LOGE(TAG, "Start failed: uploader is not ready");
        return UPLOADER_ERR_INVALID_STATE;
    }

    const BaseType_t result = xTaskCreate(uploader_task, "uploader", 4096U, context, 5U, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Start failed: could not create task");
        return UPLOADER_ERR_TASK_CREATE_FAILED;
    }

    context->started = true;
    ESP_LOGI(TAG, "Uploader task started");
    return UPLOADER_OK;
}
