#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    UPLOADER_OK = 0,
    UPLOADER_ERR_INVALID_ARGUMENT,
    UPLOADER_ERR_INVALID_STATE,
    UPLOADER_ERR_TASK_CREATE_FAILED,
} uploader_error_t;

typedef struct {
    uint32_t sensor_time;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
} acceleration_sample_t;

/**
 * Ownership rule: the recorder owns a descriptor received from free_queue. Ownership transfers
 * to the uploader only after a successful ready_queue send; the uploader returns it to free_queue
 * after processing. The sample storage must not be accessed by the former owner after transfer.
 */
typedef struct {
    acceleration_sample_t *samples;
    size_t count;
    size_t capacity;
    /** Recorder-owned acquisition metadata that travels with the sample buffer. */
    uint32_t sample_rate_hz;
    /** Wall-clock bounds sampled by the recorder during its ownership interval. */
    time_t start_time;
    time_t end_time;
} acceleration_window_t;

typedef struct {
    QueueHandle_t free_queue;
    QueueHandle_t ready_queue;
} uploader_handoff_t;

typedef struct uploader_context uploader_context_t;

/** Initialize the single uploader instance using caller-owned ownership-transfer queues. */
uploader_error_t uploader_initialize(const uploader_handoff_t *handoff, uploader_context_t **context);

/** Start the uploader task after successful initialization. */
uploader_error_t uploader_start(uploader_context_t *context);
