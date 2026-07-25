#include "shutdown_button.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "shutdown_button";
enum {
    KEY2_GPIO = GPIO_NUM_12,
    SHUTDOWN_TASK_STACK_SIZE = 2048U,
    SHUTDOWN_TASK_PRIORITY = 5U,
};
static const TickType_t DEBOUNCE_TICKS = pdMS_TO_TICKS(40);
static const TickType_t RELEASE_POLL_TICKS = pdMS_TO_TICKS(10);

struct shutdown_button_context {
    session_controller_t *session;
    TaskHandle_t task;
    bool initialized;
    bool started;
};

static shutdown_button_context_t s_context;

static void IRAM_ATTR key2_isr(void *argument)
{
    shutdown_button_context_t *const context = argument;
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (context->task != NULL) {
        vTaskNotifyGiveFromISR(context->task, &higher_priority_task_woken);
    }
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void wait_for_key_release(void)
{
    while (gpio_get_level(KEY2_GPIO) == 0) {
        vTaskDelay(RELEASE_POLL_TICKS);
    }
    (void)ulTaskNotifyTake(pdTRUE, 0U);
}

static void shutdown_task(void *argument)
{
    shutdown_button_context_t *const context = argument;

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(DEBOUNCE_TICKS);
        if (gpio_get_level(KEY2_GPIO) != 0) {
            continue;
        }

        ESP_LOGW(TAG, "KEY2 pressed; requesting PMIC shutdown");
        if (session_controller_request_manual_shutdown(context->session) != SESSION_CONTROLLER_OK) {
            ESP_LOGE(TAG, "Could not request manual shutdown");
        }
        wait_for_key_release();
    }
}

shutdown_button_error_t shutdown_button_initialize(session_controller_t *session,
                                                   shutdown_button_context_t **context)
{
    if (session == NULL || context == NULL) {
        ESP_LOGE(TAG, "initialization requires session controller and context output");
        return SHUTDOWN_BUTTON_ERR_INVALID_ARGUMENT;
    }
    if (s_context.initialized) {
        ESP_LOGE(TAG, "listener is already initialized");
        return SHUTDOWN_BUTTON_ERR_INVALID_STATE;
    }

    const gpio_config_t config = {
        .pin_bit_mask = UINT64_C(1) << KEY2_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "KEY2 GPIO configuration failed: %s", esp_err_to_name(err));
        return SHUTDOWN_BUTTON_ERR_GPIO_CONFIGURATION;
    }

    err = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "KEY2 ISR service installation failed: %s", esp_err_to_name(err));
        return SHUTDOWN_BUTTON_ERR_GPIO_ISR;
    }
    err = gpio_isr_handler_add(KEY2_GPIO, key2_isr, &s_context);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "KEY2 ISR handler installation failed: %s", esp_err_to_name(err));
        return SHUTDOWN_BUTTON_ERR_GPIO_ISR;
    }

    s_context.session = session;
    s_context.initialized = true;
    *context = &s_context;
    return SHUTDOWN_BUTTON_OK;
}

shutdown_button_error_t shutdown_button_start(shutdown_button_context_t *context)
{
    if (context == NULL) {
        ESP_LOGE(TAG, "start requires a context");
        return SHUTDOWN_BUTTON_ERR_INVALID_ARGUMENT;
    }
    if (context != &s_context || !context->initialized || context->started) {
        ESP_LOGE(TAG, "start requested in invalid state");
        return SHUTDOWN_BUTTON_ERR_INVALID_STATE;
    }

    if (xTaskCreate(shutdown_task, "shutdown_button", SHUTDOWN_TASK_STACK_SIZE, context,
                    SHUTDOWN_TASK_PRIORITY, &context->task) != pdPASS) {
        ESP_LOGE(TAG, "KEY2 shutdown task creation failed");
        return SHUTDOWN_BUTTON_ERR_TASK;
    }
    context->started = true;
    ESP_LOGI(TAG, "KEY2 listener enabled on GPIO%d", KEY2_GPIO);
    return SHUTDOWN_BUTTON_OK;
}
