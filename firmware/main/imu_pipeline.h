#pragma once

#include "imu_data.h"

typedef enum {
    IMU_PIPELINE_OK = 0,
    IMU_PIPELINE_ERR_INVALID_ARGUMENT,
    IMU_PIPELINE_ERR_INVALID_STATE,
    IMU_PIPELINE_ERR_TASK_CREATE_FAILED,
} imu_pipeline_error_t;

typedef struct imu_pipeline_context imu_pipeline_context_t;

/** Start processing completed windows before they are transferred to the uploader. */
imu_pipeline_error_t imu_pipeline_initialize(const imu_buffer_pool_t *pool,
                                             imu_pipeline_context_t **context);
imu_pipeline_error_t imu_pipeline_start(imu_pipeline_context_t *context);
