#pragma once

#include <stddef.h>
#include <stdint.h>

#define BMI270_MIN_SAMPLE_RATE_HZ 1U
#define BMI270_MAX_SAMPLE_RATE_HZ 1600U
#define BMI270_DEFAULT_CONFIG() { .sample_rate_hz = 100U }

typedef enum {
    BMI270_OK = 0,
    BMI270_ERR_INVALID_ARGUMENT,
    BMI270_ERR_INVALID_CONFIG,
    BMI270_ERR_NO_MEMORY,
    BMI270_ERR_TIMEOUT,
    BMI270_ERR_INVALID_STATE,
} bmi270_error_t;

typedef struct {
    uint32_t sample_rate_hz;
} bmi270_config_t;

typedef uint32_t bmi270_data_flags_t;

enum {
    BMI270_DATA_ACCEL_VALID = UINT32_C(1) << 0,
    BMI270_DATA_GYRO_VALID = UINT32_C(1) << 1,
    BMI270_DATA_TEMPERATURE_VALID = UINT32_C(1) << 2,
    BMI270_DATA_FIFO_OVERRUN = UINT32_C(1) << 3,
};

/** A raw dummy BMI270 sample: all units are raw unscaled values*/
typedef struct {
    uint32_t sensor_time;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temperature;
    bmi270_data_flags_t flags;
} bmi270_data_t;

typedef struct bmi270_handle bmi270_handle_t;

/**
 * Create a dummy BMI270 handle. `config` and `handle` must be non-null; on failure `*handle` is null.
 * Handles are not safe for simultaneous access by multiple tasks.
 */
bmi270_error_t bmi270_open(const bmi270_config_t *config, bmi270_handle_t **handle);

/**
 * Fill up to `capacity` samples after one simulated sample interval. `timeout_ms` of zero waits forever.
 * `samples_read` is required and is zeroed before any error after validation of that pointer.
 */
bmi270_error_t bmi270_read(bmi270_handle_t *handle, bmi270_data_t *samples, size_t capacity,
                           size_t *samples_read, uint32_t timeout_ms);

/** Enable dummy any-motion detection. This only updates handle state. */
bmi270_error_t bmi270_enable_anymotion_detection(bmi270_handle_t *handle);

/** Disable dummy any-motion detection. This only updates handle state. */
bmi270_error_t bmi270_disable_anymotion_detection(bmi270_handle_t *handle);

/** Release a handle created by bmi270_open. Passing null is invalid. */
bmi270_error_t bmi270_close(bmi270_handle_t *handle);
