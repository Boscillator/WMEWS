#pragma once

#include "bmi270.h"
#include "imu_data.h"
#include "session_controller.h"

typedef enum {
    DATA_RECORDER_OK = 0,
    DATA_RECORDER_ERR_INVALID_ARGUMENT,
    DATA_RECORDER_ERR_INVALID_STATE,
    DATA_RECORDER_ERR_UNSUPPORTED_SAMPLE_RATE,
    DATA_RECORDER_ERR_QUEUE_CREATE_FAILED,
    DATA_RECORDER_ERR_QUEUE_SEED_FAILED,
    DATA_RECORDER_ERR_IDLE_DETECTOR,
    DATA_RECORDER_ERR_TASK_CREATE_FAILED,
} data_recorder_error_t;

/** Initialize the two static acquisition buffers and their ownership-transfer queues. */
data_recorder_error_t data_recorder_initialize(imu_buffer_pool_t *pool);

/** Start the recorder task. The recorder is the only task that accesses `sensor`. */
data_recorder_error_t data_recorder_start(bmi270_handle_t *sensor, const imu_buffer_pool_t *pool,
                                          session_controller_t *session);
