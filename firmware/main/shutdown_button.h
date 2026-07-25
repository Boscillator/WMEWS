#pragma once

#include "session_controller.h"

typedef enum {
    SHUTDOWN_BUTTON_OK = 0,
    SHUTDOWN_BUTTON_ERR_INVALID_ARGUMENT,
    SHUTDOWN_BUTTON_ERR_INVALID_STATE,
    SHUTDOWN_BUTTON_ERR_GPIO_CONFIGURATION,
    SHUTDOWN_BUTTON_ERR_GPIO_ISR,
    SHUTDOWN_BUTTON_ERR_TASK,
    SHUTDOWN_BUTTON_ERR_PMIC_SHUTDOWN,
} shutdown_button_error_t;

typedef struct shutdown_button_context shutdown_button_context_t;

/** Configure the KEY2 listener using the application-owned session controller. */
shutdown_button_error_t shutdown_button_initialize(session_controller_t *session,
                                                   shutdown_button_context_t **context);
/** Start the KEY2 shutdown task after its listener has been initialized. */
shutdown_button_error_t shutdown_button_start(shutdown_button_context_t *context);
