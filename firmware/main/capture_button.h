#pragma once

#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    CAPTURE_BUTTON_OK = 0,
    CAPTURE_BUTTON_ERR_INVALID_ARGUMENT,
    CAPTURE_BUTTON_ERR_INVALID_STATE,
    CAPTURE_BUTTON_ERR_QUEUE_CREATE_FAILED,
    CAPTURE_BUTTON_ERR_GPIO_CONFIGURATION,
    CAPTURE_BUTTON_ERR_GPIO_ISR,
    CAPTURE_BUTTON_ERR_TASK,
} capture_button_error_t;

typedef struct capture_button_context capture_button_context_t;

/** Configure the KEY1 press listener and return its recorder-consumed event queue. */
capture_button_error_t capture_button_initialize(capture_button_context_t **context,
                                                 QueueHandle_t *press_queue);

/** Start the KEY1 listener task after initialization. */
capture_button_error_t capture_button_start(capture_button_context_t *context);
