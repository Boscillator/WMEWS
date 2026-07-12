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

/** Extensible metadata used to build an NDJSON stream header. */
typedef struct {
    const char *start_time;
    const char *firmware_version;
    uint32_t sample_rate_hz;
} uploader_json_metadata_t;

/** Return true only when all bytes have been accepted by the output transport. */
typedef bool (*uploader_json_writer_t)(const uint8_t *bytes, size_t length, void *context);

uploader_json_error_t uploader_json_emit_header(const uploader_json_metadata_t *metadata,
                                                uploader_json_writer_t writer, void *writer_context);
uploader_json_error_t uploader_json_emit_sample(const acceleration_sample_t *sample,
                                                uploader_json_writer_t writer, void *writer_context);
uploader_json_error_t uploader_json_emit_footer(const char *end_time, uploader_json_writer_t writer,
                                                void *writer_context);
