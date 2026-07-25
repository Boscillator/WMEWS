#include "feature_pipeline.h"

#include <limits.h>

static uint32_t axis_delta(int16_t current, int16_t previous)
{
    const int32_t delta = (int32_t)current - (int32_t)previous;
    return (uint32_t)(delta < 0 ? -delta : delta);
}

static uint32_t saturate_u64(uint64_t value)
{
    return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
}

feature_pipeline_error_t feature_pipeline_compute(const imu_window_t *window,
                                                  imu_feature_vector_t *features)
{
    if (window == NULL || features == NULL || window->samples == NULL || window->count == 0U ||
        window->count > window->capacity) {
        return FEATURE_PIPELINE_ERR_INVALID_ARGUMENT;
    }

    int64_t sum_x = 0;
    int64_t sum_y = 0;
    int64_t sum_z = 0;
    uint64_t square_x = 0U;
    uint64_t square_y = 0U;
    uint64_t square_z = 0U;
    uint32_t max_delta = 0U;

    for (size_t index = 0U; index < window->count; ++index) {
        const acceleration_sample_t *const sample = &window->samples[index];
        sum_x += sample->accel_x;
        sum_y += sample->accel_y;
        sum_z += sample->accel_z;
        square_x += (int32_t)sample->accel_x * (int32_t)sample->accel_x;
        square_y += (int32_t)sample->accel_y * (int32_t)sample->accel_y;
        square_z += (int32_t)sample->accel_z * (int32_t)sample->accel_z;
        if (index != 0U) {
            const acceleration_sample_t *const previous = &window->samples[index - 1U];
            const uint32_t delta_x = axis_delta(sample->accel_x, previous->accel_x);
            const uint32_t delta_y = axis_delta(sample->accel_y, previous->accel_y);
            const uint32_t delta_z = axis_delta(sample->accel_z, previous->accel_z);
            if (delta_x > max_delta) max_delta = delta_x;
            if (delta_y > max_delta) max_delta = delta_y;
            if (delta_z > max_delta) max_delta = delta_z;
        }
    }

    *features = (imu_feature_vector_t){
        .sample_count = (uint32_t)window->count,
        .mean_accel_x = (int32_t)(sum_x / (int64_t)window->count),
        .mean_accel_y = (int32_t)(sum_y / (int64_t)window->count),
        .mean_accel_z = (int32_t)(sum_z / (int64_t)window->count),
        .mean_square_accel_x = saturate_u64(square_x / window->count),
        .mean_square_accel_y = saturate_u64(square_y / window->count),
        .mean_square_accel_z = saturate_u64(square_z / window->count),
        .max_axis_delta_lsb = max_delta,
    };
    return FEATURE_PIPELINE_OK;
}
