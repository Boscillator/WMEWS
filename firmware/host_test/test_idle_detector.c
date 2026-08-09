#include <string.h>

#include "idle_detector.h"
#include "unity.h"

static acceleration_sample_t make_sample(int16_t x, int16_t y, int16_t z)
{
    return (acceleration_sample_t){
        .accel_x = x,
        .accel_y = y,
        .accel_z = z,
    };
}

static void test_initialize_rejects_invalid_arguments(void)
{
    idle_detector_t detector;

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ERR_INVALID_ARGUMENT,
                      idle_detector_initialize(NULL, 1U, 1U));
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ERR_INVALID_ARGUMENT,
                      idle_detector_initialize(&detector, 1U, 0U));
}

static void test_initialize_resets_detector_state(void)
{
    idle_detector_t detector;
    memset(&detector, 0xA5, sizeof(detector));

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_initialize(&detector, 7U, 3U));
    TEST_ASSERT_EQUAL_UINT16(7U, detector.max_axis_delta_lsb);
    TEST_ASSERT_EQUAL_UINT32(3U, detector.required_quiet_samples);
    TEST_ASSERT_EQUAL_UINT32(0U, detector.quiet_samples);
    TEST_ASSERT_FALSE(detector.has_previous);
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ACTIVE, detector.state);
}

static void test_update_rejects_invalid_arguments(void)
{
    idle_detector_t detector;
    const acceleration_sample_t sample = make_sample(0, 0, 0);
    bool state_changed = true;

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ERR_INVALID_ARGUMENT,
                      idle_detector_update(NULL, &sample, &state_changed));
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ERR_INVALID_ARGUMENT,
                      idle_detector_update(&detector, NULL, &state_changed));
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ERR_INVALID_ARGUMENT,
                      idle_detector_update(&detector, &sample, NULL));
}

static void test_first_sample_establishes_baseline_without_state_change(void)
{
    idle_detector_t detector;
    const acceleration_sample_t sample = make_sample(100, -200, 300);
    bool state_changed = true;

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_initialize(&detector, 5U, 2U));
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_update(&detector, &sample, &state_changed));
    TEST_ASSERT_FALSE(state_changed);
    TEST_ASSERT_TRUE(detector.has_previous);
    TEST_ASSERT_EQUAL_INT16(100, detector.previous.accel_x);
    TEST_ASSERT_EQUAL_UINT32(0U, detector.quiet_samples);
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ACTIVE, detector.state);
}

static void test_quiet_samples_at_threshold_transition_to_idle(void)
{
    idle_detector_t detector;
    bool state_changed;

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_initialize(&detector, 5U, 2U));
    const acceleration_sample_t first = make_sample(100, 200, -300);
    const acceleration_sample_t threshold_delta = make_sample(105, 195, -295);
    const acceleration_sample_t quiet = make_sample(103, 198, -290);

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_update(&detector, &first, &state_changed));
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK,
                      idle_detector_update(&detector, &threshold_delta, &state_changed));
    TEST_ASSERT_FALSE(state_changed);
    TEST_ASSERT_EQUAL_UINT32(1U, detector.quiet_samples);
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ACTIVE, detector.state);

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_update(&detector, &quiet, &state_changed));
    TEST_ASSERT_TRUE(state_changed);
    TEST_ASSERT_EQUAL_UINT32(2U, detector.quiet_samples);
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_IDLE, detector.state);

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_update(&detector, &quiet, &state_changed));
    TEST_ASSERT_FALSE(state_changed);
    TEST_ASSERT_EQUAL_UINT32(2U, detector.quiet_samples);
}

static void test_motion_resets_quiet_count_and_reactivates_detector(void)
{
    idle_detector_t detector;
    bool state_changed;
    const acceleration_sample_t still = make_sample(10, 20, 30);
    const acceleration_sample_t motion = make_sample(16, 20, 30);

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_initialize(&detector, 5U, 1U));
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_update(&detector, &still, &state_changed));
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_update(&detector, &still, &state_changed));
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_IDLE, detector.state);

    TEST_ASSERT_EQUAL(IDLE_DETECTOR_OK, idle_detector_update(&detector, &motion, &state_changed));
    TEST_ASSERT_TRUE(state_changed);
    TEST_ASSERT_EQUAL(IDLE_DETECTOR_ACTIVE, detector.state);
    TEST_ASSERT_EQUAL_UINT32(0U, detector.quiet_samples);
    TEST_ASSERT_EQUAL_INT16(16, detector.previous.accel_x);
}

void run_idle_detector_tests(void)
{
    RUN_TEST(test_initialize_rejects_invalid_arguments);
    RUN_TEST(test_initialize_resets_detector_state);
    RUN_TEST(test_update_rejects_invalid_arguments);
    RUN_TEST(test_first_sample_establishes_baseline_without_state_change);
    RUN_TEST(test_quiet_samples_at_threshold_transition_to_idle);
    RUN_TEST(test_motion_resets_quiet_count_and_reactivates_detector);
}
