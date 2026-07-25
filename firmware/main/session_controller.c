#include "session_controller.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "session";
enum { SESSION_QUEUE_LENGTH = 4U, SESSION_TASK_STACK_SIZE = 3072U };
static const TickType_t POWER_OFF_RETRY_TICKS = pdMS_TO_TICKS(1000);

typedef enum {
    SESSION_EVENT_IDLE_REQUEST,
    SESSION_EVENT_IDLE_CANCEL,
    SESSION_EVENT_FINAL_WINDOW_QUEUED,
    SESSION_EVENT_FINAL_UPLOAD_COMPLETE,
    SESSION_EVENT_MANUAL_SHUTDOWN,
} session_event_t;

struct session_controller {
    power_handle_t *power;
    QueueHandle_t event_queue;
    bool initialized;
    bool started;
};

static session_controller_t s_context;

/** `power_off` returns only on failure; retry because acquisition may already be stopped. */
static void power_off_until_success(power_handle_t *power, const char *reason)
{
    for (;;) {
        const power_error_t result = power_off(power);
        ESP_LOGE(TAG, "%s PMIC shutdown failed: %d; retrying", reason, result);
        vTaskDelay(POWER_OFF_RETRY_TICKS);
    }
}

static void session_task(void *argument)
{
    session_controller_t *const context = argument;
    bool idle_requested = false;
    bool final_window_queued = false;
    bool final_upload_complete = false;

    for (;;) {
        session_event_t event;
        if (xQueueReceive(context->event_queue, &event, portMAX_DELAY) != pdPASS) continue;

        if (event == SESSION_EVENT_MANUAL_SHUTDOWN) {
            ESP_LOGW(TAG, "Manual shutdown requested");
            power_off_until_success(context->power, "Manual");
        }
        if (event == SESSION_EVENT_IDLE_REQUEST) {
            idle_requested = true;
            ESP_LOGI(TAG, "Idle shutdown pending at capture boundary");
        } else if (event == SESSION_EVENT_IDLE_CANCEL) {
            idle_requested = false;
            final_window_queued = false;
            final_upload_complete = false;
            ESP_LOGI(TAG, "Idle shutdown cancelled by resumed motion");
        } else if (event == SESSION_EVENT_FINAL_WINDOW_QUEUED) {
            final_window_queued = true;
        } else if (event == SESSION_EVENT_FINAL_UPLOAD_COMPLETE) {
            final_upload_complete = true;
        }

        if (idle_requested && final_window_queued && final_upload_complete) {
            ESP_LOGW(TAG, "Idle capture complete; requesting PMIC shutdown");
            power_off_until_success(context->power, "Idle");
        }
    }
}

static session_controller_error_t send_event(session_controller_t *context, session_event_t event)
{
    if (context != &s_context || !context->started) {
        ESP_LOGE(TAG, "Session event requested in invalid state");
        return SESSION_CONTROLLER_ERR_INVALID_STATE;
    }
    if (xQueueSend(context->event_queue, &event, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Could not queue session event");
        return SESSION_CONTROLLER_ERR_INVALID_STATE;
    }
    return SESSION_CONTROLLER_OK;
}

session_controller_error_t session_controller_initialize(power_handle_t *power,
                                                         session_controller_t **context)
{
    if (power == NULL || context == NULL) {
        ESP_LOGE(TAG, "Initialization requires PMIC and context output");
        return SESSION_CONTROLLER_ERR_INVALID_ARGUMENT;
    }
    if (s_context.initialized) {
        ESP_LOGE(TAG, "Session controller is already initialized");
        return SESSION_CONTROLLER_ERR_INVALID_STATE;
    }
    s_context.event_queue = xQueueCreate(SESSION_QUEUE_LENGTH, sizeof(session_event_t));
    if (s_context.event_queue == NULL) {
        ESP_LOGE(TAG, "Could not create session event queue");
        return SESSION_CONTROLLER_ERR_QUEUE_CREATE_FAILED;
    }
    s_context.power = power;
    s_context.initialized = true;
    *context = &s_context;
    return SESSION_CONTROLLER_OK;
}

session_controller_error_t session_controller_start(session_controller_t *context)
{
    if (context != &s_context || !context->initialized || context->started) {
        ESP_LOGE(TAG, "Start requested in invalid state");
        return SESSION_CONTROLLER_ERR_INVALID_STATE;
    }
    if (xTaskCreate(session_task, "session", SESSION_TASK_STACK_SIZE, context, 5U, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not create session task");
        return SESSION_CONTROLLER_ERR_TASK_CREATE_FAILED;
    }
    context->started = true;
    ESP_LOGI(TAG, "Session controller started");
    return SESSION_CONTROLLER_OK;
}

session_controller_error_t session_controller_request_idle_shutdown(session_controller_t *context)
{
    return send_event(context, SESSION_EVENT_IDLE_REQUEST);
}

session_controller_error_t session_controller_cancel_idle_shutdown(session_controller_t *context)
{
    return send_event(context, SESSION_EVENT_IDLE_CANCEL);
}

session_controller_error_t session_controller_notify_final_window_queued(session_controller_t *context)
{
    return send_event(context, SESSION_EVENT_FINAL_WINDOW_QUEUED);
}

session_controller_error_t session_controller_notify_final_upload_complete(session_controller_t *context)
{
    return send_event(context, SESSION_EVENT_FINAL_UPLOAD_COMPLETE);
}

session_controller_error_t session_controller_request_manual_shutdown(session_controller_t *context)
{
    return send_event(context, SESSION_EVENT_MANUAL_SHUTDOWN);
}
