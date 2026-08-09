#include <limits.h>

#include "feature_pipeline.h"
#include "unity.h"

static void test_compute_rejects_invalid_arguments(void)
{
    acceleration_sample_t samples[1] = {0};
    imu_feature_vector_t features;
    const imu_window_t valid_window = {
        .samples = samples,
        .count = 1U,
        .capacity = 1U,
    };
    const imu_window_t empty_window = {
        .samples = samples,
        .count = 0U,
        .capacity = 1U,
    };
    const imu_window_t missing_samples_window = {
        .count = 1U,
        .capacity = 1U,
    };
    const imu_window_t oversized_window = {
        .samples = samples,
        .count = 2U,
        .capacity = 1U,
    };

    TEST_ASSERT_EQUAL(FEATURE_PIPELINE_ERR_INVALID_ARGUMENT,
                      feature_pipeline_compute(NULL, &features));
    TEST_ASSERT_EQUAL(FEATURE_PIPELINE_ERR_INVALID_ARGUMENT,
                      feature_pipeline_compute(&valid_window, NULL));
    TEST_ASSERT_EQUAL(FEATURE_PIPELINE_ERR_INVALID_ARGUMENT,
                      feature_pipeline_compute(&empty_window, &features));
    TEST_ASSERT_EQUAL(FEATURE_PIPELINE_ERR_INVALID_ARGUMENT,
                      feature_pipeline_compute(&missing_samples_window, &features));
    TEST_ASSERT_EQUAL(FEATURE_PIPELINE_ERR_INVALID_ARGUMENT,
                      feature_pipeline_compute(&oversized_window, &features));
}

static void test_compute_features_for_multiple_samples(void)
{
    acceleration_sample_t samples[] = {
        {.sensor_time = 1U, .accel_x = 1, .accel_y = 10, .accel_z = -20},
        {.sensor_time = 2U, .accel_x = -2, .accel_y = 13, .accel_z = -17},
        {.sensor_time = 3U, .accel_x = 4, .accel_y = 8, .accel_z = -23},
    };
    const imu_window_t window = {
        .samples = samples,
        .count = 3U,
        .capacity = 3U,
    };
    imu_feature_vector_t features;

    TEST_ASSERT_EQUAL(FEATURE_PIPELINE_OK, feature_pipeline_compute(&window, &features));
    TEST_ASSERT_EQUAL_UINT32(3U, features.sample_count);
    TEST_ASSERT_EQUAL_INT32(1, features.mean_accel_x);
    TEST_ASSERT_EQUAL_INT32(10, features.mean_accel_y);
    TEST_ASSERT_EQUAL_INT32(-20, features.mean_accel_z);
    TEST_ASSERT_EQUAL_UINT32(7U, features.mean_square_accel_x);
    TEST_ASSERT_EQUAL_UINT32(111U, features.mean_square_accel_y);
    TEST_ASSERT_EQUAL_UINT32(406U, features.mean_square_accel_z);
    TEST_ASSERT_EQUAL_UINT32(6U, features.max_axis_delta_lsb);
}

static void test_compute_single_sample_has_zero_delta(void)
{
    acceleration_sample_t sample = {.accel_x = -7, .accel_y = 8, .accel_z = -9};
    const imu_window_t window = {
        .samples = &sample,
        .count = 1U,
        .capacity = 1U,
    };
    imu_feature_vector_t features;

    TEST_ASSERT_EQUAL(FEATURE_PIPELINE_OK, feature_pipeline_compute(&window, &features));
    TEST_ASSERT_EQUAL_UINT32(1U, features.sample_count);
    TEST_ASSERT_EQUAL_INT32(-7, features.mean_accel_x);
    TEST_ASSERT_EQUAL_INT32(8, features.mean_accel_y);
    TEST_ASSERT_EQUAL_INT32(-9, features.mean_accel_z);
    TEST_ASSERT_EQUAL_UINT32(49U, features.mean_square_accel_x);
    TEST_ASSERT_EQUAL_UINT32(64U, features.mean_square_accel_y);
    TEST_ASSERT_EQUAL_UINT32(81U, features.mean_square_accel_z);
    TEST_ASSERT_EQUAL_UINT32(0U, features.max_axis_delta_lsb);
}

static void test_compute_handles_extreme_signed_acceleration_values(void)
{
    acceleration_sample_t samples[] = {
        {.accel_x = INT16_MIN, .accel_y = INT16_MAX, .accel_z = INT16_MIN},
        {.accel_x = INT16_MAX, .accel_y = INT16_MIN, .accel_z = INT16_MAX},
    };
    const imu_window_t window = {
        .samples = samples,
        .count = 2U,
        .capacity = 2U,
    };
    imu_feature_vector_t features;

    TEST_ASSERT_EQUAL(FEATURE_PIPELINE_OK, feature_pipeline_compute(&window, &features));
    TEST_ASSERT_EQUAL_INT32(0, features.mean_accel_x);
    TEST_ASSERT_EQUAL_INT32(0, features.mean_accel_y);
    TEST_ASSERT_EQUAL_INT32(0, features.mean_accel_z);
    TEST_ASSERT_EQUAL_UINT32(1073709056U, features.mean_square_accel_x);
    TEST_ASSERT_EQUAL_UINT32(1073709056U, features.mean_square_accel_y);
    TEST_ASSERT_EQUAL_UINT32(1073709056U, features.mean_square_accel_z);
    TEST_ASSERT_EQUAL_UINT32(65535U, features.max_axis_delta_lsb);
}

void run_feature_pipeline_tests(void)
{
    RUN_TEST(test_compute_rejects_invalid_arguments);
    RUN_TEST(test_compute_features_for_multiple_samples);
    RUN_TEST(test_compute_single_sample_has_zero_delta);
    RUN_TEST(test_compute_handles_extreme_signed_acceleration_values);
}
