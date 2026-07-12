#pragma once

#include "bmi270.h"
#include "uploader.h"

typedef enum {
    DATA_RECORDER_OK = 0,
    DATA_RECORDER_ERR_INVALID_ARGUMENT,
    DATA_RECORDER_ERR_INVALID_STATE,
    DATA_RECORDER_ERR_UNSUPPORTED_SAMPLE_RATE,
    DATA_RECORDER_ERR_QUEUE_CREATE_FAILED,
    DATA_RECORDER_ERR_QUEUE_SEED_FAILED,
    DATA_RECORDER_ERR_TASK_CREATE_FAILED,
} data_recorder_error_t;

/** Initialize the two static acquisition buffers and their ownership-transfer queues. */
data_recorder_error_t data_recorder_initialize(uploader_handoff_t *handoff);

/** Start the recorder task. The recorder is the only task that accesses `sensor`. */
data_recorder_error_t data_recorder_start(bmi270_handle_t *sensor, const uploader_handoff_t *handoff);
