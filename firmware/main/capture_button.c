#include "capture_button.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "capture_button";
enum {
    KEY1_GPIO = GPIO_NUM_11,
    CAPTURE_BUTTON_EVENT_QUEUE_LENGTH = 8U,
    CAPTURE_BUTTON_TASK_STACK_SIZE = 2048U,
    CAPTURE_BUTTON_TASK_PRIORITY = 5U,
};
static const TickType_t DEBOUNCE_TICKS = pdMS_TO_TICKS(40);
static const TickType_t RELEASE_POLL_TICKS = pdMS_TO_TICKS(10);

struct capture_button_context {
    QueueHandle_t press_queue;
    TaskHandle_t task;
    bool initialized;
    bool started;
};

static capture_button_context_t s_context;
static StaticQueue_t s_press_queue_storage;
static uint8_t s_press_queue_buffer[CAPTURE_BUTTON_EVENT_QUEUE_LENGTH * sizeof(time_t)];

static void IRAM_ATTR key1_isr(void *argument)
{
    capture_button_context_t *const context = argument;
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
    while (gpio_get_level(KEY1_GPIO) == 0) {
        vTaskDelay(RELEASE_POLL_TICKS);
    }
    (void)ulTaskNotifyTake(pdTRUE, 0U);
}

static void capture_button_task(void *argument)
{
    capture_button_context_t *const context = argument;

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(DEBOUNCE_TICKS);
        if (gpio_get_level(KEY1_GPIO) != 0) {
            continue;
        }

        const time_t pressed_at = time(NULL);
        if (pressed_at == (time_t)-1) {
            ESP_LOGE(TAG, "Could not timestamp KEY1 press");
        } else if (xQueueSend(context->press_queue, &pressed_at, 0U) != pdPASS) {
            ESP_LOGE(TAG, "Could not queue KEY1 press timestamp");
        } else {
            ESP_LOGI(TAG, "KEY1 press recorded");
        }
        wait_for_key_release();
    }
}

capture_button_error_t capture_button_initialize(capture_button_context_t **context,
                                                 QueueHandle_t *press_queue)
{
    if (context == NULL || press_queue == NULL) {
        ESP_LOGE(TAG, "Initialization requires context and queue outputs");
        return CAPTURE_BUTTON_ERR_INVALID_ARGUMENT;
    }
    if (s_context.initialized) {
        ESP_LOGE(TAG, "Listener is already initialized");
        return CAPTURE_BUTTON_ERR_INVALID_STATE;
    }

    s_context.press_queue = xQueueCreateStatic(CAPTURE_BUTTON_EVENT_QUEUE_LENGTH, sizeof(time_t),
                                                s_press_queue_buffer, &s_press_queue_storage);
    if (s_context.press_queue == NULL) {
        ESP_LOGE(TAG, "Could not create KEY1 press queue");
        return CAPTURE_BUTTON_ERR_QUEUE_CREATE_FAILED;
    }

    const gpio_config_t config = {
        .pin_bit_mask = UINT64_C(1) << KEY1_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "KEY1 GPIO configuration failed: %s", esp_err_to_name(err));
        return CAPTURE_BUTTON_ERR_GPIO_CONFIGURATION;
    }

    err = gpio_isr_handler_add(KEY1_GPIO, key1_isr, &s_context);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "KEY1 ISR handler installation failed: %s", esp_err_to_name(err));
        return CAPTURE_BUTTON_ERR_GPIO_ISR;
    }

    s_context.initialized = true;
    *context = &s_context;
    *press_queue = s_context.press_queue;
    return CAPTURE_BUTTON_OK;
}

capture_button_error_t capture_button_start(capture_button_context_t *context)
{
    if (context == NULL) {
        ESP_LOGE(TAG, "Start requires a context");
        return CAPTURE_BUTTON_ERR_INVALID_ARGUMENT;
    }
    if (context != &s_context || !context->initialized || context->started) {
        ESP_LOGE(TAG, "Start requested in invalid state");
        return CAPTURE_BUTTON_ERR_INVALID_STATE;
    }

    if (xTaskCreate(capture_button_task, "capture_button", CAPTURE_BUTTON_TASK_STACK_SIZE, context,
                    CAPTURE_BUTTON_TASK_PRIORITY, &context->task) != pdPASS) {
        ESP_LOGE(TAG, "KEY1 listener task creation failed");
        return CAPTURE_BUTTON_ERR_TASK;
    }
    context->started = true;
    ESP_LOGI(TAG, "KEY1 listener enabled on GPIO%d", KEY1_GPIO);
    return CAPTURE_BUTTON_OK;
}
