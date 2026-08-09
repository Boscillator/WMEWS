cmake_minimum_required(VERSION 3.16)

project(wmews_host_tests C)

set(IDF_PATH "${CMAKE_CURRENT_LIST_DIR}/../../esp-idf")

add_executable(wmews_host_tests
    "test_dummy.c"
    "../main/idle_detector.c"
    "../main/feature_pipeline.c"
    "${IDF_PATH}/components/unity/unity/src/unity.c")

target_include_directories(wmews_host_tests PRIVATE
    "include"
    "../main"
    "${IDF_PATH}/components/unity/unity/src")

include(CTest)
add_test(NAME wmews_host_tests COMMAND wmews_host_tests)
