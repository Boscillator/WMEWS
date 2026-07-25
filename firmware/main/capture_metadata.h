#pragma once

#include "imu_data.h"
#include "power.h"
#include "uploader_json.h"

/** Errors returned while collecting the uploader's bounded metadata snapshot. */
typedef enum {
    CAPTURE_METADATA_OK = 0,
    CAPTURE_METADATA_ERR_INVALID_ARGUMENT,
} capture_metadata_error_t;

/** Collect one bounded, current upload metadata snapshot into caller-owned storage. */
capture_metadata_error_t capture_metadata_collect(const imu_buffer_pool_t *pool, power_handle_t *power,
                                                  uploader_json_metadata_t *metadata);
