#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"

#define BMI270_I2C_ADDRESS_DEFAULT 0x68U
#define BMI270_I2C_CLOCK_HZ_DEFAULT 400000U
#define BMI270_MIN_SAMPLE_RATE_HZ 25U
#define BMI270_MAX_SAMPLE_RATE_HZ 1600U
#define BMI270_DEFAULT_CONFIG(bus_) \
    { .bus = (bus_), .i2c_address = BMI270_I2C_ADDRESS_DEFAULT, \
      .i2c_clock_hz = BMI270_I2C_CLOCK_HZ_DEFAULT, .sample_rate_hz = 100U }

typedef enum {
    BMI270_OK = 0,
    BMI270_ERR_INVALID_ARGUMENT,
    BMI270_ERR_INVALID_CONFIG,
    BMI270_ERR_INVALID_STATE,
    BMI270_ERR_TRANSPORT,
    BMI270_ERR_DEVICE_ID,
    BMI270_ERR_INITIALIZATION,
    BMI270_ERR_FIFO,
    BMI270_ERR_TIMEOUT,
} bmi270_error_t;

typedef struct {
    i2c_master_bus_handle_t bus; /**< Borrowed application-owned I2C bus. */
    uint8_t i2c_address;         /**< 7-bit BMI270 address. */
    uint32_t i2c_clock_hz;
    uint32_t sample_rate_hz;
} bmi270_config_t;

typedef uint32_t bmi270_data_flags_t;

enum {
    BMI270_DATA_ACCEL_VALID = UINT32_C(1) << 0,
    BMI270_DATA_GYRO_VALID = UINT32_C(1) << 1,
    BMI270_DATA_TEMPERATURE_VALID = UINT32_C(1) << 2,
    BMI270_DATA_FIFO_OVERRUN = UINT32_C(1) << 3,
};

/** Raw BMI270 sample. Sensor time is the wrapping 24-bit, 39.0625 us/tick counter.
 * Acceleration is ±8 g (4096 LSB/g), gyro is ±2000 dps (16.384 LSB/dps), and
 * valid temperature converts as 23 C + raw / 512. */
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

/** Return the configured hardware ODR, or zero unless the driver is live. */
uint32_t bmi270_get_sample_rate_hz(const bmi270_handle_t *handle);
/** Return the exact integer sensor-time interval in microseconds, or zero unless live. */
uint32_t bmi270_get_sensor_time_dt_us(const bmi270_handle_t *handle);
float bmi270_get_gyro_lsb(const bmi270_handle_t *handle);
float bmi270_get_accel_lsb(const bmi270_handle_t *handle);
float bmi270_get_temperature_lsb(const bmi270_handle_t *handle);

/** Add and initialize the BMI270 on the supplied application-owned bus. */
bmi270_error_t bmi270_open(const bmi270_config_t *config, bmi270_handle_t **handle);
/** Read complete FIFO samples. A timeout of zero waits indefinitely; partial frames are never returned. */
bmi270_error_t bmi270_read(bmi270_handle_t *handle, bmi270_data_t *samples, size_t capacity,
                           size_t *samples_read, uint32_t timeout_ms);
/** Disable the sensor, remove its device handle, and leave the borrowed bus intact. */
bmi270_error_t bmi270_close(bmi270_handle_t *handle);
