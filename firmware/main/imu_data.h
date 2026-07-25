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

/** A fixed acquisition buffer whose ownership moves through the IMU pipeline queues. */
typedef struct {
    acceleration_sample_t *samples;
    size_t count;
    size_t capacity;
    uint32_t sample_rate_hz;
    time_t start_time;
    time_t end_time;
    /** The controller powers down after this window has completed its upload attempt. */
    bool shutdown_after_upload;
} imu_window_t;

/** Queues that implement exclusive ownership: free -> pipeline -> upload -> free. */
typedef struct {
    QueueHandle_t free_queue;
    QueueHandle_t pipeline_queue;
    QueueHandle_t upload_queue;
} imu_buffer_pool_t;
