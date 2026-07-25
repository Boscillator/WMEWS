#include "capture_metadata.h"

#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "capture_metadata";
enum { ELF_SHA256_BYTES = 32U };

static void format_mac(const uint8_t mac[6], char output[UPLOADER_JSON_MAC_STRING_SIZE])
{
    (void)snprintf(output, UPLOADER_JSON_MAC_STRING_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1],
                   mac[2], mac[3], mac[4], mac[5]);
}

static void format_elf_sha(const uint8_t sha[ELF_SHA256_BYTES],
                           char output[UPLOADER_JSON_ELF_SHA_STRING_SIZE])
{
    for (size_t index = 0U; index < ELF_SHA256_BYTES; ++index) {
        (void)snprintf(&output[index * 2U], 3U, "%02x", sha[index]);
    }
}

capture_metadata_error_t capture_metadata_collect(const imu_buffer_pool_t *pool, power_handle_t *power,
                                                  uploader_json_metadata_t *metadata)
{
    if (pool == NULL || power == NULL || metadata == NULL || pool->free_queue == NULL ||
        pool->pipeline_queue == NULL || pool->upload_queue == NULL) {
        ESP_LOGE(TAG, "Metadata collection requires a complete buffer pool, PMIC, and output");
        return CAPTURE_METADATA_ERR_INVALID_ARGUMENT;
    }

    uint16_t battery_mv = 0U;
    const power_error_t battery_result = power_read_battery_mv(power, &battery_mv);
    metadata->battery.read_ok = battery_result == POWER_OK;
    metadata->battery.millivolts = battery_mv;
    if (battery_result != POWER_OK) {
        ESP_LOGE(TAG, "Battery metadata read failed: %d", battery_result);
    }

    const int64_t uptime_us = esp_timer_get_time();
    metadata->system.uptime_ms = uptime_us < 0 ? 0U : (uint64_t)uptime_us / 1000U;
    metadata->system.snapshot_timestamp_ms = metadata->system.uptime_ms;
    metadata->system.tick_count = (uint64_t)xTaskGetTickCount();
    metadata->system.tick_rate_hz = configTICK_RATE_HZ;
    metadata->system.task_count = uxTaskGetNumberOfTasks();
    metadata->system.uploader_min_free_stack_bytes =
        (uint32_t)uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
    metadata->system.free_queue_depth = uxQueueMessagesWaiting(pool->free_queue);
    metadata->system.pipeline_queue_depth = uxQueueMessagesWaiting(pool->pipeline_queue);
    metadata->system.upload_queue_depth = uxQueueMessagesWaiting(pool->upload_queue);
    metadata->system.heap_free_bytes = esp_get_free_heap_size();
    metadata->system.heap_min_free_bytes = esp_get_minimum_free_heap_size();
    metadata->system.heap_largest_free_block_bytes =
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    metadata->system.reset_reason = (uint32_t)esp_reset_reason();

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    metadata->device.model = (uint32_t)chip_info.model;
    metadata->device.revision = chip_info.revision;
    (void)snprintf(metadata->device.target, sizeof(metadata->device.target), "%s", CONFIG_IDF_TARGET);
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_BASE) == ESP_OK) {
        format_mac(mac, metadata->device.base_mac);
    } else {
        ESP_LOGE(TAG, "Could not read base MAC");
        metadata->device.base_mac[0] = '\0';
    }

    const esp_app_desc_t *const app = esp_app_get_description();
    if (app == NULL) {
        ESP_LOGE(TAG, "Could not read application description");
        return CAPTURE_METADATA_ERR_INVALID_ARGUMENT;
    }
    (void)snprintf(metadata->device.project_name, sizeof(metadata->device.project_name), "%s", app->project_name);
    (void)snprintf(metadata->build.firmware_version, sizeof(metadata->build.firmware_version), "%s", app->version);
    (void)snprintf(metadata->build.idf_version, sizeof(metadata->build.idf_version), "%s", esp_get_idf_version());
    (void)snprintf(metadata->build.date, sizeof(metadata->build.date), "%s", app->date);
    (void)snprintf(metadata->build.time, sizeof(metadata->build.time), "%s", app->time);
    format_elf_sha(app->app_elf_sha256, metadata->build.elf_sha256);
    metadata->firmware_version = metadata->build.firmware_version;
    return CAPTURE_METADATA_OK;
}
