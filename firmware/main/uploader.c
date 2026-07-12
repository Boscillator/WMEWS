#include "uploader.h"

#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "uploader_json.h"

static const char *TAG = "uploader";
static const TickType_t RETURN_RETRY_TICKS = pdMS_TO_TICKS(1000);

struct uploader_context {
    uploader_handoff_t handoff;
    bool initialized;
    bool started;
};

static uploader_context_t s_context;

static bool format_utc_timestamp(char *timestamp, size_t timestamp_size)
{
    time_t now;
    struct tm utc_time;

    if (time(&now) == (time_t)-1 || gmtime_r(&now, &utc_time) == NULL ||
        strftime(timestamp, timestamp_size, "%Y-%m-%dT%H:%M:%SZ", &utc_time) == 0U) {
        ESP_LOGE(TAG, "Could not format UTC timestamp");
        return false;
    }
    return true;
}

static bool printf_writer(const uint8_t *bytes, size_t length, void *context)
{
    (void)context;
    if (bytes == NULL || length > (size_t)INT_MAX) {
        ESP_LOGE(TAG, "stdout writer received invalid output length");
        return false;
    }

    const int written = printf("%.*s", (int)length, (const char *)bytes);
    if (written != (int)length) {
        ESP_LOGE(TAG, "stdout writer failed: wrote %d of %u bytes", written, (unsigned)length);
        return false;
    }
    return true;
}

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

        if (window.samples == NULL || window.count == 0U || window.count > window.capacity ||
            window.sample_rate_hz == 0U) {
            ESP_LOGE(TAG, "Received invalid window: samples=%p count=%u capacity=%u rate=%uHz",
                     (void *)window.samples, (unsigned)window.count, (unsigned)window.capacity,
                     (unsigned)window.sample_rate_hz);
            return_window_or_retry(context, &window);
            continue;
        }

        char start_time[sizeof("YYYY-MM-DDTHH:MM:SSZ")];
        const esp_app_desc_t *app_description = esp_app_get_description();
        if (!format_utc_timestamp(start_time, sizeof(start_time)) || app_description == NULL ||
            app_description->version[0] == '\0') {
            ESP_LOGE(TAG, "Could not prepare stream header metadata");
            return_window_or_retry(context, &window);
            continue;
        }

        const uploader_json_metadata_t metadata = {
            .start_time = start_time,
            .firmware_version = app_description->version,
            .sample_rate_hz = window.sample_rate_hz,
        };
        uploader_json_error_t json_result =
            uploader_json_emit_header(&metadata, printf_writer, NULL);
        if (json_result != UPLOADER_JSON_OK) {
            ESP_LOGE(TAG, "Header emission failed: %d", json_result);
            return_window_or_retry(context, &window);
            continue;
        }

        for (size_t index = 0; index < window.count; ++index) {
            json_result = uploader_json_emit_sample(&window.samples[index], printf_writer, NULL);
            if (json_result != UPLOADER_JSON_OK) {
                ESP_LOGE(TAG, "Sample %u emission failed: %d", (unsigned)index, json_result);
                break;
            }
        }

        if (json_result == UPLOADER_JSON_OK) {
            char end_time[sizeof("YYYY-MM-DDTHH:MM:SSZ")];
            if (format_utc_timestamp(end_time, sizeof(end_time))) {
                json_result = uploader_json_emit_footer(end_time, printf_writer, NULL);
                if (json_result != UPLOADER_JSON_OK) {
                    ESP_LOGE(TAG, "Footer emission failed: %d", json_result);
                }
            }
        }
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
