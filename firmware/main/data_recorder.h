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

/** Application-selected idle-detection thresholds used by the recorder task. */
typedef struct {
    uint16_t max_axis_delta_lsb;
    uint32_t duration_seconds;
} data_recorder_config_t;

/** Initialize the two static acquisition buffers and their ownership-transfer queues. */
data_recorder_error_t data_recorder_initialize(imu_buffer_pool_t *pool);

/** Start the recorder task with the supplied idle-detection configuration.
 * The recorder is the only task that accesses `sensor`. */
data_recorder_error_t data_recorder_start(bmi270_handle_t *sensor, const imu_buffer_pool_t *pool,
                                          session_controller_t *session, QueueHandle_t button_press_queue,
                                          const data_recorder_config_t *config);
