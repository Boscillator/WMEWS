#pragma once

#include "imu_data.h"
#include "power.h"
#include "session_controller.h"

typedef enum {
    UPLOADER_OK = 0,
    UPLOADER_ERR_INVALID_ARGUMENT,
    UPLOADER_ERR_INVALID_STATE,
    UPLOADER_ERR_TASK_CREATE_FAILED,
} uploader_error_t;

typedef struct uploader_context uploader_context_t;

/** Initialize the single uploader instance using caller-owned queues and PMIC control. */
uploader_error_t uploader_initialize(const imu_buffer_pool_t *pool, power_handle_t *power,
                                     session_controller_t *session, uploader_context_t **context);

/** Start the uploader task after successful initialization. */
uploader_error_t uploader_start(uploader_context_t *context);
