#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uploader.h"

typedef enum {
    UPLOADER_JSON_OK = 0,
    UPLOADER_JSON_ERR_INVALID_ARGUMENT,
    UPLOADER_JSON_ERR_ALLOCATION,
    UPLOADER_JSON_ERR_SERIALIZATION,
    UPLOADER_JSON_ERR_WRITE,
} uploader_json_error_t;

enum {
    UPLOADER_JSON_MAC_STRING_SIZE = 18U,
    UPLOADER_JSON_TARGET_STRING_SIZE = 24U,
    UPLOADER_JSON_PROJECT_NAME_SIZE = 33U,
    UPLOADER_JSON_VERSION_STRING_SIZE = 33U,
    UPLOADER_JSON_IDF_VERSION_STRING_SIZE = 33U,
    UPLOADER_JSON_BUILD_DATE_STRING_SIZE = 17U,
    UPLOADER_JSON_BUILD_TIME_STRING_SIZE = 17U,
    UPLOADER_JSON_ELF_SHA_STRING_SIZE = 65U,
};

/** Battery measurement captured immediately before the upload-size calculation. */
typedef struct {
    bool read_ok;
    uint16_t millivolts;
} uploader_json_battery_t;

/** Bounded FreeRTOS and heap health snapshot captured by the uploader task. */
typedef struct {
    uint64_t uptime_ms;
    uint64_t snapshot_timestamp_ms;
    uint64_t tick_count;
    uint32_t tick_rate_hz;
    uint32_t task_count;
    uint32_t uploader_min_free_stack_bytes;
    uint32_t free_queue_depth;
    uint32_t pipeline_queue_depth;
    uint32_t upload_queue_depth;
    uint32_t heap_free_bytes;
    uint32_t heap_min_free_bytes;
    uint32_t heap_largest_free_block_bytes;
    uint32_t reset_reason;
} uploader_json_system_t;

/** Hardware identity copied into bounded caller-owned storage. */
typedef struct {
    char target[UPLOADER_JSON_TARGET_STRING_SIZE];
    uint32_t model;
    uint32_t revision;
    char base_mac[UPLOADER_JSON_MAC_STRING_SIZE];
    char project_name[UPLOADER_JSON_PROJECT_NAME_SIZE];
} uploader_json_device_t;

/** Build identity copied into bounded caller-owned storage. */
typedef struct {
    char firmware_version[UPLOADER_JSON_VERSION_STRING_SIZE];
    char idf_version[UPLOADER_JSON_IDF_VERSION_STRING_SIZE];
    char date[UPLOADER_JSON_BUILD_DATE_STRING_SIZE];
    char time[UPLOADER_JSON_BUILD_TIME_STRING_SIZE];
    char elf_sha256[UPLOADER_JSON_ELF_SHA_STRING_SIZE];
} uploader_json_build_t;

/** Extensible metadata used to build an NDJSON stream header. */
typedef struct {
    const char *start_time;
    /** Optional ISO-8601 UTC timestamp for the first accepted KEY1 press. */
    const char *button_pressed_at;
    const char *firmware_version;
    uint32_t sample_rate_hz;
    const imu_feature_vector_t *features;
    uploader_json_battery_t battery;
    uploader_json_system_t system;
    uploader_json_device_t device;
    uploader_json_build_t build;
} uploader_json_metadata_t;

/** Return true only when all bytes have been accepted by the output transport. */
typedef bool (*uploader_json_writer_t)(const uint8_t *bytes, size_t length, void *context);

/** Serialize one NDJSON header record from already-collected metadata. */
uploader_json_error_t uploader_json_emit_header(const uploader_json_metadata_t *metadata,
                                                uploader_json_writer_t writer, void *writer_context);
/** Serialize one compact raw acceleration sample record. */
uploader_json_error_t uploader_json_emit_sample(const acceleration_sample_t *sample,
                                                uploader_json_writer_t writer, void *writer_context);
/** Serialize the terminal NDJSON footer record. */
uploader_json_error_t uploader_json_emit_footer(const char *end_time, uploader_json_writer_t writer,
                                                void *writer_context);
