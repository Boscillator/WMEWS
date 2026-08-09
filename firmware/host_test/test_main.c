#include "unity.h"

void run_idle_detector_tests(void);
void run_feature_pipeline_tests(void);

void setUp(void) {}

void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    run_idle_detector_tests();
    run_feature_pipeline_tests();
    return UNITY_END();
}
