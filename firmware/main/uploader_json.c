#include "uploader_json.h"

#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "uploader_json";
enum { JSON_OUTPUT_BUFFER_SIZE = 256U };

static uploader_json_error_t emit_json(cJSON *record, uploader_json_writer_t writer, void *writer_context)
{
    if (record == NULL || writer == NULL) {
        ESP_LOGE(TAG, "Emit failed: invalid record or writer");
        cJSON_Delete(record);
        return UPLOADER_JSON_ERR_INVALID_ARGUMENT;
    }

    char output[JSON_OUTPUT_BUFFER_SIZE];
    const bool serialized = cJSON_PrintPreallocated(record, output, sizeof(output), false);
    cJSON_Delete(record);
    if (!serialized) {
        ESP_LOGE(TAG, "Emit failed: record exceeds %u byte output buffer", (unsigned)JSON_OUTPUT_BUFFER_SIZE);
        return UPLOADER_JSON_ERR_SERIALIZATION;
    }

    const size_t output_length = strlen(output);
    if (!writer((const uint8_t *)output, output_length, writer_context) ||
        !writer((const uint8_t *)"\n", 1U, writer_context)) {
        ESP_LOGE(TAG, "Emit failed: writer rejected record output");
        return UPLOADER_JSON_ERR_WRITE;
    }
    return UPLOADER_JSON_OK;
}

uploader_json_error_t uploader_json_emit_header(const uploader_json_metadata_t *metadata,
                                                uploader_json_writer_t writer, void *writer_context)
{
    if (metadata == NULL || metadata->start_time == NULL || metadata->firmware_version == NULL || writer == NULL) {
        ESP_LOGE(TAG, "Header failed: invalid metadata or writer");
        return UPLOADER_JSON_ERR_INVALID_ARGUMENT;
    }

    cJSON *record = cJSON_CreateObject();
    if (record == NULL || cJSON_AddStringToObject(record, "start_time", metadata->start_time) == NULL ||
        cJSON_AddStringToObject(record, "firmware_version", metadata->firmware_version) == NULL ||
        cJSON_AddNumberToObject(record, "sample_rate_hz", (double)metadata->sample_rate_hz) == NULL) {
        ESP_LOGE(TAG, "Header failed: could not allocate JSON record");
        cJSON_Delete(record);
        return UPLOADER_JSON_ERR_ALLOCATION;
    }
    return emit_json(record, writer, writer_context);
}

uploader_json_error_t uploader_json_emit_sample(const acceleration_sample_t *sample,
                                                uploader_json_writer_t writer, void *writer_context)
{
    if (sample == NULL || writer == NULL) {
        ESP_LOGE(TAG, "Sample failed: invalid sample or writer");
        return UPLOADER_JSON_ERR_INVALID_ARGUMENT;
    }

    cJSON *record = cJSON_CreateObject();
    if (record == NULL || cJSON_AddNumberToObject(record, "t", (double)sample->sensor_time) == NULL ||
        cJSON_AddNumberToObject(record, "x", (double)sample->accel_x) == NULL ||
        cJSON_AddNumberToObject(record, "y", (double)sample->accel_y) == NULL ||
        cJSON_AddNumberToObject(record, "z", (double)sample->accel_z) == NULL) {
        ESP_LOGE(TAG, "Sample failed: could not allocate JSON record");
        cJSON_Delete(record);
        return UPLOADER_JSON_ERR_ALLOCATION;
    }
    return emit_json(record, writer, writer_context);
}

uploader_json_error_t uploader_json_emit_footer(const char *end_time, uploader_json_writer_t writer,
                                                void *writer_context)
{
    if (end_time == NULL || writer == NULL) {
        ESP_LOGE(TAG, "Footer failed: invalid end time or writer");
        return UPLOADER_JSON_ERR_INVALID_ARGUMENT;
    }

    cJSON *record = cJSON_CreateObject();
    if (record == NULL || cJSON_AddStringToObject(record, "end_time", end_time) == NULL) {
        ESP_LOGE(TAG, "Footer failed: could not allocate JSON record");
        cJSON_Delete(record);
        return UPLOADER_JSON_ERR_ALLOCATION;
    }
    return emit_json(record, writer, writer_context);
}
