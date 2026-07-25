#pragma once

#include <stdint.h>

#include "imu_data.h"

typedef enum {
    FEATURE_PIPELINE_OK = 0,
    FEATURE_PIPELINE_ERR_INVALID_ARGUMENT,
} feature_pipeline_error_t;

/** Stable per-window features for the future local model interface. */
typedef struct {
    uint32_t sample_count;
    int32_t mean_accel_x;
    int32_t mean_accel_y;
    int32_t mean_accel_z;
    uint32_t mean_square_accel_x;
    uint32_t mean_square_accel_y;
    uint32_t mean_square_accel_z;
    uint32_t max_axis_delta_lsb;
} imu_feature_vector_t;

feature_pipeline_error_t feature_pipeline_compute(const imu_window_t *window,
                                                  imu_feature_vector_t *features);
