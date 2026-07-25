#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "imu_data.h"

typedef enum {
    IDLE_DETECTOR_OK = 0,
    IDLE_DETECTOR_ERR_INVALID_ARGUMENT,
} idle_detector_error_t;

typedef enum {
    IDLE_DETECTOR_ACTIVE = 0,
    IDLE_DETECTOR_IDLE,
} idle_detector_state_t;

typedef struct {
    acceleration_sample_t previous;
    uint32_t quiet_samples;
    uint32_t required_quiet_samples;
    uint16_t max_axis_delta_lsb;
    bool has_previous;
    idle_detector_state_t state;
} idle_detector_t;

/** Initialize a delta-based detector. A quiet period must contain at least one sample. */
idle_detector_error_t idle_detector_initialize(idle_detector_t *detector,
                                               uint16_t max_axis_delta_lsb,
                                               uint32_t required_quiet_samples);
/** Update the detector and report whether its active/idle state changed. */
idle_detector_error_t idle_detector_update(idle_detector_t *detector,
                                           const acceleration_sample_t *sample, bool *state_changed);
