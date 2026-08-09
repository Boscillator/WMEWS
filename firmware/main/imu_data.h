#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
    uint32_t sensor_time;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
} acceleration_sample_t;

/** Stable per-window features computed by the feature-pipeline task. */
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

/** A fixed acquisition buffer whose ownership moves through the IMU pipeline queues. */
typedef struct {
    acceleration_sample_t *samples;
    size_t count;
    size_t capacity;
    uint32_t sample_rate_hz;
    uint8_t accel_range_g;
    uint16_t accel_lsb_per_g;
    time_t start_time;
    time_t end_time;
    /** First accepted KEY1 press timestamp, or `(time_t)-1` if no press occurred. */
    time_t button_pressed_at;
    /** The controller powers down after this window has completed its upload attempt. */
    bool shutdown_after_upload;
    /** Features calculated for this exact window by the feature pipeline. */
    imu_feature_vector_t features;
    /** True only after the feature pipeline has populated `features`. */
    bool features_valid;
} imu_window_t;

/** Queues that implement exclusive ownership: free -> pipeline -> upload -> free. */
typedef struct {
    QueueHandle_t free_queue;
    QueueHandle_t pipeline_queue;
    QueueHandle_t upload_queue;
} imu_buffer_pool_t;
