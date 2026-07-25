#include "idle_detector.h"

#include <stdlib.h>

static uint32_t axis_delta(int16_t current, int16_t previous)
{
    const int32_t delta = (int32_t)current - (int32_t)previous;
    return (uint32_t)(delta < 0 ? -delta : delta);
}

idle_detector_error_t idle_detector_initialize(idle_detector_t *detector,
                                               uint16_t max_axis_delta_lsb,
                                               uint32_t required_quiet_samples)
{
    if (detector == NULL || required_quiet_samples == 0U) {
        return IDLE_DETECTOR_ERR_INVALID_ARGUMENT;
    }

    *detector = (idle_detector_t){
        .max_axis_delta_lsb = max_axis_delta_lsb,
        .required_quiet_samples = required_quiet_samples,
        .state = IDLE_DETECTOR_ACTIVE,
    };
    return IDLE_DETECTOR_OK;
}

idle_detector_error_t idle_detector_update(idle_detector_t *detector,
                                           const acceleration_sample_t *sample, bool *state_changed)
{
    if (detector == NULL || sample == NULL || state_changed == NULL) {
        return IDLE_DETECTOR_ERR_INVALID_ARGUMENT;
    }

    *state_changed = false;
    if (!detector->has_previous) {
        detector->previous = *sample;
        detector->has_previous = true;
        return IDLE_DETECTOR_OK;
    }

    const bool quiet = axis_delta(sample->accel_x, detector->previous.accel_x) <= detector->max_axis_delta_lsb &&
                       axis_delta(sample->accel_y, detector->previous.accel_y) <= detector->max_axis_delta_lsb &&
                       axis_delta(sample->accel_z, detector->previous.accel_z) <= detector->max_axis_delta_lsb;
    detector->previous = *sample;

    if (!quiet) {
        detector->quiet_samples = 0U;
        if (detector->state == IDLE_DETECTOR_IDLE) {
            detector->state = IDLE_DETECTOR_ACTIVE;
            *state_changed = true;
        }
        return IDLE_DETECTOR_OK;
    }

    if (detector->quiet_samples < detector->required_quiet_samples) {
        ++detector->quiet_samples;
    }
    if (detector->state == IDLE_DETECTOR_ACTIVE &&
        detector->quiet_samples == detector->required_quiet_samples) {
        detector->state = IDLE_DETECTOR_IDLE;
        *state_changed = true;
    }
    return IDLE_DETECTOR_OK;
}
