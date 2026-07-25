#pragma once

#include "power.h"

typedef enum {
    SESSION_CONTROLLER_OK = 0,
    SESSION_CONTROLLER_ERR_INVALID_ARGUMENT,
    SESSION_CONTROLLER_ERR_INVALID_STATE,
    SESSION_CONTROLLER_ERR_QUEUE_CREATE_FAILED,
    SESSION_CONTROLLER_ERR_TASK_CREATE_FAILED,
} session_controller_error_t;

typedef struct session_controller session_controller_t;

/** Initialize the sole owner of the application shutdown transition. */
session_controller_error_t session_controller_initialize(power_handle_t *power,
                                                         session_controller_t **context);
session_controller_error_t session_controller_start(session_controller_t *context);
/** Request shutdown after the recorder's current completed window is uploaded. */
session_controller_error_t session_controller_request_idle_shutdown(session_controller_t *context);
/** Cancel a pending idle shutdown when motion resumes before the capture boundary. */
session_controller_error_t session_controller_cancel_idle_shutdown(session_controller_t *context);
/** Report that the final capture has been handed to the processing pipeline. */
session_controller_error_t session_controller_notify_final_window_queued(session_controller_t *context);
/** Report completion of the final capture's upload attempt, including failures. */
session_controller_error_t session_controller_notify_final_upload_complete(session_controller_t *context);
/** Request an immediate user-initiated shutdown. */
session_controller_error_t session_controller_request_manual_shutdown(session_controller_t *context);
