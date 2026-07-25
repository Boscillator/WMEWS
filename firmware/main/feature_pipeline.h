#pragma once

#include <stdint.h>

#include "imu_data.h"

typedef enum {
    FEATURE_PIPELINE_OK = 0,
    FEATURE_PIPELINE_ERR_INVALID_ARGUMENT,
} feature_pipeline_error_t;

/** Compute stable features for one complete IMU window. */
feature_pipeline_error_t feature_pipeline_compute(const imu_window_t *window,
                                                  imu_feature_vector_t *features);
