#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

static void test_host_test_framework_is_available(void)
{
    TEST_ASSERT_TRUE(true);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_host_test_framework_is_available, __LINE__);
    return UNITY_END();
}
