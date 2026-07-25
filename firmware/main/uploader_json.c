#include "uploader_json.h"

#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "uploader_json";
enum {
    JSON_RECORD_OUTPUT_BUFFER_SIZE = 256U,
    JSON_HEADER_OUTPUT_BUFFER_SIZE = 4096U,
    SENSOR_TIME_WRAP_TICKS = 1U << 24,
};

static uploader_json_error_t emit_json(cJSON *record, size_t buffer_size, uploader_json_writer_t writer,
                                       void *writer_context)
{
    if (record == NULL || writer == NULL) {
        ESP_LOGE(TAG, "Emit failed: invalid record or writer");
        cJSON_Delete(record);
        return UPLOADER_JSON_ERR_INVALID_ARGUMENT;
    }

    char output[JSON_HEADER_OUTPUT_BUFFER_SIZE];
    const bool serialized = buffer_size <= sizeof(output) &&
                            cJSON_PrintPreallocated(record, output, (int)buffer_size, false);
    cJSON_Delete(record);
    if (!serialized) {
        ESP_LOGE(TAG, "Emit failed: record exceeds %u byte output buffer", (unsigned)buffer_size);
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

static bool add_features(cJSON *record, const imu_feature_vector_t *features)
{
    cJSON *const object = cJSON_AddObjectToObject(record, "features");
    return object != NULL && cJSON_AddNumberToObject(object, "sample_count", features->sample_count) != NULL &&
           cJSON_AddNumberToObject(object, "mean_accel_x_lsb", features->mean_accel_x) != NULL &&
           cJSON_AddNumberToObject(object, "mean_accel_y_lsb", features->mean_accel_y) != NULL &&
           cJSON_AddNumberToObject(object, "mean_accel_z_lsb", features->mean_accel_z) != NULL &&
           cJSON_AddNumberToObject(object, "mean_square_accel_x_lsb2", features->mean_square_accel_x) != NULL &&
           cJSON_AddNumberToObject(object, "mean_square_accel_y_lsb2", features->mean_square_accel_y) != NULL &&
           cJSON_AddNumberToObject(object, "mean_square_accel_z_lsb2", features->mean_square_accel_z) != NULL &&
           cJSON_AddNumberToObject(object, "max_axis_delta_lsb", features->max_axis_delta_lsb) != NULL;
}

static bool add_data_format(cJSON *record)
{
    cJSON *const format = cJSON_AddObjectToObject(record, "data_format");
    cJSON *const raw = format == NULL ? NULL : cJSON_AddObjectToObject(format, "raw");
    cJSON *const time = raw == NULL ? NULL : cJSON_AddObjectToObject(raw, "t");
    cJSON *const x = raw == NULL ? NULL : cJSON_AddObjectToObject(raw, "x");
    cJSON *const y = raw == NULL ? NULL : cJSON_AddObjectToObject(raw, "y");
    cJSON *const z = raw == NULL ? NULL : cJSON_AddObjectToObject(raw, "z");
    cJSON *const features = format == NULL ? NULL : cJSON_AddObjectToObject(format, "features");
    return time != NULL && x != NULL && y != NULL && z != NULL && features != NULL &&
           cJSON_AddStringToObject(time, "unit", "sensor_ticks") != NULL &&
           cJSON_AddNumberToObject(time, "tick_duration_us", 39.0625) != NULL &&
           cJSON_AddNumberToObject(time, "wrap_ticks", SENSOR_TIME_WRAP_TICKS) != NULL &&
           cJSON_AddStringToObject(x, "unit", "lsb") != NULL &&
           cJSON_AddStringToObject(y, "unit", "lsb") != NULL &&
           cJSON_AddStringToObject(z, "unit", "lsb") != NULL &&
           cJSON_AddNumberToObject(x, "accelerometer_range_g", 8) != NULL &&
           cJSON_AddNumberToObject(y, "accelerometer_range_g", 8) != NULL &&
           cJSON_AddNumberToObject(z, "accelerometer_range_g", 8) != NULL &&
           cJSON_AddNumberToObject(x, "lsb_per_g", 4096) != NULL &&
           cJSON_AddNumberToObject(y, "lsb_per_g", 4096) != NULL &&
           cJSON_AddNumberToObject(z, "lsb_per_g", 4096) != NULL &&
           cJSON_AddStringToObject(features, "mean_acceleration_unit", "lsb") != NULL &&
           cJSON_AddStringToObject(features, "mean_square_acceleration_unit", "lsb2") != NULL &&
           cJSON_AddStringToObject(features, "max_axis_delta_unit", "lsb") != NULL &&
           cJSON_AddNumberToObject(features, "accelerometer_range_g", 8) != NULL &&
           cJSON_AddNumberToObject(features, "lsb_per_g", 4096) != NULL;
}

static bool add_snapshot(cJSON *record, const uploader_json_metadata_t *metadata)
{
    cJSON *const battery = cJSON_AddObjectToObject(record, "battery");
    cJSON *const system = cJSON_AddObjectToObject(record, "system");
    cJSON *const device = cJSON_AddObjectToObject(record, "device");
    cJSON *const build = cJSON_AddObjectToObject(record, "build");
    return battery != NULL && system != NULL && device != NULL && build != NULL &&
           cJSON_AddBoolToObject(battery, "read_ok", metadata->battery.read_ok) != NULL &&
           (metadata->battery.read_ok ? cJSON_AddNumberToObject(battery, "millivolts", metadata->battery.millivolts)
                                      : cJSON_AddNullToObject(battery, "millivolts")) != NULL &&
           cJSON_AddNumberToObject(system, "uptime_ms", (double)metadata->system.uptime_ms) != NULL &&
           cJSON_AddNumberToObject(system, "snapshot_timestamp_ms", (double)metadata->system.snapshot_timestamp_ms) != NULL &&
           cJSON_AddNumberToObject(system, "tick_count", (double)metadata->system.tick_count) != NULL &&
           cJSON_AddNumberToObject(system, "tick_rate_hz", metadata->system.tick_rate_hz) != NULL &&
           cJSON_AddNumberToObject(system, "task_count", metadata->system.task_count) != NULL &&
           cJSON_AddNumberToObject(system, "uploader_min_free_stack_bytes", metadata->system.uploader_min_free_stack_bytes) != NULL &&
           cJSON_AddNumberToObject(system, "free_queue_depth", metadata->system.free_queue_depth) != NULL &&
           cJSON_AddNumberToObject(system, "pipeline_queue_depth", metadata->system.pipeline_queue_depth) != NULL &&
           cJSON_AddNumberToObject(system, "upload_queue_depth", metadata->system.upload_queue_depth) != NULL &&
           cJSON_AddNumberToObject(system, "heap_free_bytes", metadata->system.heap_free_bytes) != NULL &&
           cJSON_AddNumberToObject(system, "heap_min_free_bytes", metadata->system.heap_min_free_bytes) != NULL &&
           cJSON_AddNumberToObject(system, "heap_largest_free_block_bytes", metadata->system.heap_largest_free_block_bytes) != NULL &&
           cJSON_AddNumberToObject(system, "reset_reason", metadata->system.reset_reason) != NULL &&
           cJSON_AddStringToObject(device, "target", metadata->device.target) != NULL &&
           cJSON_AddNumberToObject(device, "model", metadata->device.model) != NULL &&
           cJSON_AddNumberToObject(device, "revision", metadata->device.revision) != NULL &&
           cJSON_AddStringToObject(device, "base_mac", metadata->device.base_mac) != NULL &&
           cJSON_AddStringToObject(device, "project_name", metadata->device.project_name) != NULL &&
           cJSON_AddStringToObject(build, "firmware_version", metadata->build.firmware_version) != NULL &&
           cJSON_AddStringToObject(build, "idf_version", metadata->build.idf_version) != NULL &&
           cJSON_AddStringToObject(build, "date", metadata->build.date) != NULL &&
           cJSON_AddStringToObject(build, "time", metadata->build.time) != NULL &&
           cJSON_AddStringToObject(build, "elf_sha256", metadata->build.elf_sha256) != NULL;
}

uploader_json_error_t uploader_json_emit_header(const uploader_json_metadata_t *metadata,
                                                uploader_json_writer_t writer, void *writer_context)
{
    if (metadata == NULL || metadata->start_time == NULL || metadata->firmware_version == NULL ||
        metadata->features == NULL || writer == NULL) {
        ESP_LOGE(TAG, "Header failed: invalid metadata or writer");
        return UPLOADER_JSON_ERR_INVALID_ARGUMENT;
    }

    cJSON *record = cJSON_CreateObject();
    if (record == NULL || cJSON_AddStringToObject(record, "start_time", metadata->start_time) == NULL ||
        cJSON_AddStringToObject(record, "firmware_version", metadata->firmware_version) == NULL ||
        cJSON_AddNumberToObject(record, "sample_rate_hz", (double)metadata->sample_rate_hz) == NULL ||
        (metadata->button_pressed_at != NULL &&
         cJSON_AddStringToObject(record, "button_pressed_at", metadata->button_pressed_at) == NULL) ||
        (metadata->button_pressed_at == NULL && cJSON_AddNullToObject(record, "button_pressed_at") == NULL) ||
        !add_features(record, metadata->features) || !add_data_format(record) || !add_snapshot(record, metadata)) {
        ESP_LOGE(TAG, "Header failed: could not allocate JSON record");
        cJSON_Delete(record);
        return UPLOADER_JSON_ERR_ALLOCATION;
    }
    return emit_json(record, JSON_HEADER_OUTPUT_BUFFER_SIZE, writer, writer_context);
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
    return emit_json(record, JSON_RECORD_OUTPUT_BUFFER_SIZE, writer, writer_context);
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
    return emit_json(record, JSON_RECORD_OUTPUT_BUFFER_SIZE, writer, writer_context);
}
