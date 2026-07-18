#include "uploader.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "uploader_http.h"
#include "uploader_json.h"

static const char *TAG = "uploader";
static const TickType_t RETURN_RETRY_TICKS = pdMS_TO_TICKS(1000);
enum {
    UPLOADER_TASK_STACK_SIZE = 8192U,
    SERIAL_YIELD_INTERVAL_SAMPLES = 16U,
    UPLOADER_OUTPUT_BUFFER_SIZE = 4096U
};
static const TickType_t SERIAL_YIELD_TICKS = 1U;

struct uploader_context {
    uploader_handoff_t handoff;
    uint8_t output_buffer[UPLOADER_OUTPUT_BUFFER_SIZE];
    size_t output_length;
    uploader_json_writer_t transport_writer;
    void *transport_context;
    bool initialized;
    bool started;
};

static uploader_context_t s_context;
static config_credentials_t s_credentials;
static char s_upload_url[UPLOADER_HTTP_UPLOAD_URL_MAX_LENGTH + 1U];
static uploader_http_upload_session_t s_upload_session;

typedef struct {
    size_t length;
} counting_writer_context_t;

static bool format_utc_timestamp(time_t capture_time, char *timestamp, size_t timestamp_size)
{
    struct tm utc_time;

    if (capture_time == (time_t)-1 || gmtime_r(&capture_time, &utc_time) == NULL ||
        strftime(timestamp, timestamp_size, "%Y-%m-%dT%H:%M:%SZ", &utc_time) == 0U) {
        ESP_LOGE(TAG, "Could not format UTC timestamp");
        return false;
    }
    return true;
}

static bool counting_writer(const uint8_t *bytes, size_t length, void *context)
{
    counting_writer_context_t *const counter = context;
    if (counter == NULL || bytes == NULL || length > SIZE_MAX - counter->length) {
        ESP_LOGE(TAG, "Counting writer received invalid output length");
        return false;
    }
    counter->length += length;
    return true;
}

static bool http_writer(const uint8_t *bytes, size_t length, void *context)
{
    return uploader_http_upload_write(context, bytes, length) == UPLOADER_HTTP_OK;
}

static bool flush_output_buffer(uploader_context_t *context)
{
    if (context == NULL || context->transport_writer == NULL) {
        ESP_LOGE(TAG, "Output buffer is not initialized");
        return false;
    }
    if (context->output_length == 0U) {
        return true;
    }
    if (!context->transport_writer(context->output_buffer, context->output_length,
                                   context->transport_context)) {
        ESP_LOGE(TAG, "Output buffer flush failed for %u bytes", (unsigned)context->output_length);
        return false;
    }

    context->output_length = 0U;
    return true;
}

static bool buffered_writer(const uint8_t *bytes, size_t length, void *context)
{
    uploader_context_t *const uploader = context;
    if (uploader == NULL || bytes == NULL) {
        ESP_LOGE(TAG, "Output buffer received invalid input");
        return false;
    }
    if (length == 0U) {
        return true;
    }
    if (length > sizeof(uploader->output_buffer)) {
        if (!flush_output_buffer(uploader)) {
            return false;
        }
        if (!uploader->transport_writer(bytes, length, uploader->transport_context)) {
            ESP_LOGE(TAG, "Output transport rejected oversized %u byte fragment", (unsigned)length);
            return false;
        }
        return true;
    }
    if (length > sizeof(uploader->output_buffer) - uploader->output_length &&
        !flush_output_buffer(uploader)) {
        return false;
    }

    memcpy(&uploader->output_buffer[uploader->output_length], bytes, length);
    uploader->output_length += length;
    return true;
}

static void initialize_output_buffer(uploader_context_t *context, uploader_json_writer_t transport_writer,
                                     void *transport_context)
{
    context->output_length = 0U;
    context->transport_writer = transport_writer;
    context->transport_context = transport_context;
}

static uploader_json_error_t emit_capture(const acceleration_window_t *window,
                                          const uploader_json_metadata_t *metadata, const char *end_time,
                                          uploader_json_writer_t writer, void *writer_context)
{
    uploader_json_error_t result = uploader_json_emit_header(metadata, writer, writer_context);
    if (result != UPLOADER_JSON_OK) {
        ESP_LOGE(TAG, "Header emission failed: %d", result);
        return result;
    }

    for (size_t index = 0U; index < window->count; ++index) {
        result = uploader_json_emit_sample(&window->samples[index], writer, writer_context);
        if (result != UPLOADER_JSON_OK) {
            ESP_LOGE(TAG, "Sample %u emission failed: %d", (unsigned)index, result);
            return result;
        }
        if ((index + 1U) % SERIAL_YIELD_INTERVAL_SAMPLES == 0U) {
            vTaskDelay(SERIAL_YIELD_TICKS);
        }
    }

    result = uploader_json_emit_footer(end_time, writer, writer_context);
    if (result != UPLOADER_JSON_OK) {
        ESP_LOGE(TAG, "Footer emission failed: %d", result);
    }
    return result;
}

static void return_window_or_retry(const uploader_context_t *context, const acceleration_window_t *window)
{
    while (xQueueSend(context->handoff.free_queue, window, RETURN_RETRY_TICKS) != pdPASS) {
        ESP_LOGE(TAG, "Fatal ownership invariant violation: could not return buffer; retrying");
    }
}

static void uploader_task(void *argument)
{
    uploader_context_t *const context = argument;
    acceleration_window_t window;

    for (;;) {
        if (xQueueReceive(context->handoff.ready_queue, &window, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (window.samples == NULL || window.count == 0U || window.count > window.capacity ||
            window.sample_rate_hz == 0U || window.start_time == (time_t)-1 ||
            window.end_time == (time_t)-1) {
            ESP_LOGE(TAG,
                     "Received invalid window: samples=%p count=%u capacity=%u rate=%uHz start=%lld end=%lld",
                     (void *)window.samples, (unsigned)window.count, (unsigned)window.capacity,
                     (unsigned)window.sample_rate_hz, (long long)window.start_time,
                     (long long)window.end_time);
            return_window_or_retry(context, &window);
            continue;
        }

        char start_time[sizeof("YYYY-MM-DDTHH:MM:SSZ")];
        char end_time[sizeof("YYYY-MM-DDTHH:MM:SSZ")];
        const esp_app_desc_t *app_description = esp_app_get_description();
        if (!format_utc_timestamp(window.start_time, start_time, sizeof(start_time)) ||
            app_description == NULL || app_description->version[0] == '\0' ||
            !format_utc_timestamp(window.end_time, end_time, sizeof(end_time))) {
            ESP_LOGE(TAG, "Could not prepare stream header metadata");
            return_window_or_retry(context, &window);
            continue;
        }

        const uploader_json_metadata_t metadata = {
            .start_time = start_time,
            .firmware_version = app_description->version,
            .sample_rate_hz = window.sample_rate_hz,
        };
        counting_writer_context_t counter = {0};
        uploader_json_error_t json_result =
            emit_capture(&window, &metadata, end_time, counting_writer, &counter);
        if (json_result != UPLOADER_JSON_OK || counter.length == 0U) {
            ESP_LOGE(TAG, "Capture counting failed");
            return_window_or_retry(context, &window);
            continue;
        }

        const config_err_t config_result = config_load_credentials(&s_credentials);
        if (config_result != CONFIG_OK) {
            ESP_LOGE(TAG, "Upload URL configuration failed: %d", config_result);
            return_window_or_retry(context, &window);
            continue;
        }
        uploader_http_error_t http_result =
            uploader_http_get_upload_url(&s_credentials, s_upload_url, sizeof(s_upload_url));
        if (http_result != UPLOADER_HTTP_OK) {
            ESP_LOGE(TAG, "Upload URL request failed: %d", http_result);
            return_window_or_retry(context, &window);
            continue;
        }

        uploader_http_upload_init(&s_upload_session);
        http_result = uploader_http_upload_start(&s_upload_session, s_upload_url, counter.length);
        if (http_result != UPLOADER_HTTP_OK) {
            ESP_LOGE(TAG, "Upload start failed: %d", http_result);
            return_window_or_retry(context, &window);
            continue;
        }
        ESP_LOGI(TAG, "Upload started: %u bytes", (unsigned)counter.length);

        initialize_output_buffer(context, http_writer, &s_upload_session);
        json_result = emit_capture(&window, &metadata, end_time, buffered_writer, context);
        if (json_result == UPLOADER_JSON_OK && !flush_output_buffer(context)) {
            json_result = UPLOADER_JSON_ERR_WRITE;
            ESP_LOGE(TAG, "Final output flush failed: %d", json_result);
        }
        if (json_result != UPLOADER_JSON_OK) {
            ESP_LOGE(TAG, "Upload serialization failed: %d", json_result);
            uploader_http_upload_abort(&s_upload_session);
        } else {
            http_result = uploader_http_upload_finish(&s_upload_session);
            if (http_result == UPLOADER_HTTP_OK) {
                ESP_LOGI(TAG, "Upload completed: %u bytes", (unsigned)counter.length);
            } else {
                ESP_LOGE(TAG, "Upload failed: %d", http_result);
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

    const BaseType_t result =
        xTaskCreate(uploader_task, "uploader", UPLOADER_TASK_STACK_SIZE, context, 5U, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Start failed: could not create task");
        return UPLOADER_ERR_TASK_CREATE_FAILED;
    }

    context->started = true;
    ESP_LOGI(TAG, "Uploader task started");
    return UPLOADER_OK;
}
