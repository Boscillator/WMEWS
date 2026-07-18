#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"

#define POWER_I2C_ADDRESS_DEFAULT 0x6EU
#define POWER_I2C_CLOCK_HZ_DEFAULT 100000U
#define POWER_DEFAULT_CONFIG(bus_) \
    { .bus = (bus_), .i2c_address = POWER_I2C_ADDRESS_DEFAULT, .i2c_clock_hz = POWER_I2C_CLOCK_HZ_DEFAULT }

typedef enum {
    POWER_OK = 0,
    POWER_ERR_INVALID_ARGUMENT,
    POWER_ERR_INVALID_CONFIG,
    POWER_ERR_INVALID_STATE,
    POWER_ERR_TRANSPORT,
    POWER_ERR_DEVICE_ID,
    POWER_ERR_CONFIGURATION,
} power_error_t;

typedef uint8_t power_wake_flags_t;

enum {
    POWER_WAKE_TIMER = UINT8_C(1) << 0,
    POWER_WAKE_VIN = UINT8_C(1) << 1,
    POWER_WAKE_BUTTON = UINT8_C(1) << 2,
    POWER_WAKE_RESET_BUTTON = UINT8_C(1) << 3,
    POWER_WAKE_COMMAND_RESET = UINT8_C(1) << 4,
    POWER_WAKE_EXTERNAL = UINT8_C(1) << 5,
    POWER_WAKE_5V_INOUT = UINT8_C(1) << 6,
};

typedef enum {
    POWER_WAKE_EDGE_FALLING,
    POWER_WAKE_EDGE_RISING,
} power_wake_edge_t;

typedef enum {
    POWER_WAKE_PULL_NONE,
    POWER_WAKE_PULL_UP,
    POWER_WAKE_PULL_DOWN,
} power_wake_pull_t;

typedef struct {
    i2c_master_bus_handle_t bus; /**< Borrowed application-owned I2C bus. */
    uint8_t i2c_address;         /**< 7-bit M5PM1 I2C address. */
    uint32_t i2c_clock_hz;
} power_config_t;

typedef struct power_handle power_handle_t;

/** Add and identify the M5PM1 on the supplied application-owned I2C bus. */
power_error_t power_open(const power_config_t *config, power_handle_t **handle);
/** Read the PMIC's battery-voltage register in millivolts. */
power_error_t power_read_battery_mv(power_handle_t *handle, uint16_t *millivolts);
/** Read the PMIC wake-source flags without clearing them. */
power_error_t power_get_wake_sources(power_handle_t *handle, power_wake_flags_t *sources);
/** Clear the selected PMIC wake-source flags (write-one-to-clear). */
power_error_t power_clear_wake_sources(power_handle_t *handle, power_wake_flags_t sources);
/** Configure PMIC GPIO4 as the BMI270 motion wake source. */
power_error_t power_configure_motion_wake(power_handle_t *handle, power_wake_edge_t edge,
                                           power_wake_pull_t pull);
/** Disable PMIC GPIO4 motion wake and release its retained pull configuration. */
power_error_t power_disable_motion_wake(power_handle_t *handle);
/** Power off through the M5PM1. On success this function does not return. */
power_error_t power_off(power_handle_t *handle);
/** Remove the PMIC device handle and leave the borrowed bus intact. */
power_error_t power_close(power_handle_t *handle);
