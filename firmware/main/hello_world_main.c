#include <time.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "nvs_flash.h"
#include "driver/i2c_master.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "bmi270.h"
#include "capture_button.h"
#include "data_recorder.h"
#include "imu_pipeline.h"
#include "network.h"
#include "power.h"
#include "session_controller.h"
#include "shutdown_button.h"
#include "uploader.h"

static const char *TAG = "wmews";
static bmi270_handle_t *s_bmi270;
static power_handle_t *s_power;
static shutdown_button_context_t *s_shutdown_button;
static capture_button_context_t *s_capture_button;
static session_controller_t *s_session_controller;
static i2c_master_bus_handle_t s_i2c_bus;
static TaskHandle_t s_boot_led_task;
static StaticSemaphore_t s_boot_led_stopped_storage;
static SemaphoreHandle_t s_boot_led_stopped;
static TaskHandle_t s_normal_led_task;
static StaticSemaphore_t s_normal_led_stopped_storage;
static SemaphoreHandle_t s_normal_led_stopped;

#define SNTP_SYNC_RETRIES 5
#define SNTP_SYNC_WAIT_MS 5000
#define BOOT_LED_HALF_PERIOD_MS 100
#define NORMAL_LED_FLASH_MS 75
#define NORMAL_LED_PERIOD_MS 3000

static void boot_led_task(void *argument)
{
    power_handle_t *const power = argument;
    bool enabled = false;

    for (;;) {
        enabled = !enabled;
        if (power_set_status_led(power, enabled) != POWER_OK) break;
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BOOT_LED_HALF_PERIOD_MS)) != 0U) break;
    }

    (void)power_set_status_led(power, false);
    (void)xSemaphoreGive(s_boot_led_stopped);
    vTaskDelete(NULL);
}

static bool start_boot_led(void)
{
    s_boot_led_stopped = xSemaphoreCreateBinaryStatic(&s_boot_led_stopped_storage);
    if (s_boot_led_stopped == NULL) {
        ESP_LOGE(TAG, "Boot LED completion semaphore initialization failed");
        return false;
    }
    if (xTaskCreate(boot_led_task, "boot_led", 2048U, s_power, 4U, &s_boot_led_task) != pdPASS) {
        ESP_LOGE(TAG, "Boot LED task creation failed");
        s_boot_led_task = NULL;
        return false;
    }
    return true;
}

static void stop_boot_led(void)
{
    if (s_boot_led_task == NULL) return;

    (void)xTaskNotifyGive(s_boot_led_task);
    (void)xSemaphoreTake(s_boot_led_stopped, portMAX_DELAY);
    s_boot_led_task = NULL;
}

static void normal_led_task(void *argument)
{
    power_handle_t *const power = argument;
    TickType_t next_flash = xTaskGetTickCount();

    for (;;) {
        if (power_flash_status_led(power, NORMAL_LED_FLASH_MS, 1U) != POWER_OK) break;
        if (ulTaskNotifyTake(pdTRUE, 0U) != 0U) break;
        vTaskDelayUntil(&next_flash, pdMS_TO_TICKS(NORMAL_LED_PERIOD_MS));
        if (ulTaskNotifyTake(pdTRUE, 0U) != 0U) break;
    }

    (void)power_set_status_led(power, false);
    (void)xSemaphoreGive(s_normal_led_stopped);
    vTaskDelete(NULL);
}

static bool start_normal_led(void)
{
    s_normal_led_stopped = xSemaphoreCreateBinaryStatic(&s_normal_led_stopped_storage);
    if (s_normal_led_stopped == NULL) {
        ESP_LOGE(TAG, "Normal LED completion semaphore initialization failed");
        return false;
    }
    if (xTaskCreate(normal_led_task, "normal_led", 2048U, s_power, 4U, &s_normal_led_task) != pdPASS) {
        ESP_LOGE(TAG, "Normal LED task creation failed");
        s_normal_led_task = NULL;
        return false;
    }
    return true;
}

static void stop_normal_led(void)
{
    if (s_normal_led_task == NULL) return;

    (void)xTaskNotifyGive(s_normal_led_task);
    (void)xSemaphoreTake(s_normal_led_stopped, portMAX_DELAY);
    s_normal_led_task = NULL;
}

static esp_err_t initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err != ESP_ERR_NVS_NO_FREE_PAGES && err != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        return err;
    }

    ESP_LOGW(TAG, "NVS is full or incompatible; erasing it before retrying initialization");
    err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGW(TAG, "NVS erased; device credentials must be reprovisioned");
    return nvs_flash_init();
}

static esp_err_t initialize_i2c_bus(void)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_47,
        .scl_io_num = GPIO_NUM_48,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    const esp_err_t err = i2c_new_master_bus(&config, &s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus initialization failed: %s", esp_err_to_name(err));
    }
    return err;
}

static void cleanup_sensor_bus(void)
{
    stop_normal_led();
    stop_boot_led();
    if (s_power != NULL) {
        (void)power_set_status_led(s_power, false);
        (void)power_close(s_power);
        s_power = NULL;
    }
    if (s_bmi270 != NULL) {
        (void)bmi270_close(s_bmi270);
        s_bmi270 = NULL;
    }
    if (s_i2c_bus != NULL) {
        const esp_err_t err = i2c_del_master_bus(s_i2c_bus);
        if (err != ESP_OK) ESP_LOGE(TAG, "I2C bus cleanup failed: %s", esp_err_to_name(err));
        s_i2c_bus = NULL;
    }
}

void app_main(void)
{
    ESP_LOGD(TAG, "Initializing NVS");
    esp_err_t err = initialize_nvs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(err));
        return;
    }

    if (initialize_i2c_bus() != ESP_OK) {
        return;
    }

    const power_config_t power_config = POWER_DEFAULT_CONFIG(s_i2c_bus);
    const power_error_t power_result = power_open(&power_config, &s_power);
    if (power_result != POWER_OK) {
        ESP_LOGE(TAG, "M5PM1 initialization failed: %d", power_result);
        cleanup_sensor_bus();
        return;
    }
    if (!start_boot_led()) {
        cleanup_sensor_bus();
        return;
    }

    if (initialize_wifi() != NETWORK_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization failed");
        cleanup_sensor_bus();
        return;
    }

    ESP_LOGD(TAG, "Initializing SNTP with pool.ntp.org");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP initialization failed: %s", esp_err_to_name(err));
        cleanup_sensor_bus();
        return;
    }

    for (int attempt = 1; attempt <= SNTP_SYNC_RETRIES; ++attempt) {
        err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(SNTP_SYNC_WAIT_MS));
        if (err == ESP_OK) {
            break;
        }
        if (attempt < SNTP_SYNC_RETRIES) {
            ESP_LOGW(TAG, "Waiting for SNTP synchronization (%d/%d). Failed: %s", attempt, SNTP_SYNC_RETRIES, esp_err_to_name(err));
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP synchronization failed: %s", esp_err_to_name(err));
        cleanup_sensor_bus();
        return;
    }

    time_t now;
    struct tm utc_time;
    char timestamp[sizeof("YYYY-MM-DDTHH:MM:SSZ")];
    time(&now);
    if (gmtime_r(&now, &utc_time) == NULL || strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", &utc_time) == 0) {
        ESP_LOGE(TAG, "Could not format synchronized UTC time");
        cleanup_sensor_bus();
        return;
    }
    ESP_LOGI(TAG, "Synchronized UTC time: %s", timestamp);

    power_wake_flags_t wake_sources;
    if (power_get_wake_sources(s_power, &wake_sources) != POWER_OK) {
        ESP_LOGE(TAG, "M5PM1 wake-source read failed");
        cleanup_sensor_bus();
        return;
    }
    if ((wake_sources & POWER_WAKE_EXTERNAL) != 0U) {
        ESP_LOGI(TAG, "Wake reason: PMIC external GPIO (motion candidate)");
    }
    if (wake_sources != 0U && power_clear_wake_sources(s_power, wake_sources) != POWER_OK) {
        ESP_LOGE(TAG, "M5PM1 wake-source clear failed");
        cleanup_sensor_bus();
        return;
    }

    uint16_t battery_mv;
    if (power_read_battery_mv(s_power, &battery_mv) != POWER_OK) {
        ESP_LOGE(TAG, "M5PM1 battery read failed");
        cleanup_sensor_bus();
        return;
    }
    ESP_LOGI(TAG, "Battery voltage: %u mV", (unsigned)battery_mv);

    const bmi270_config_t sensor_config = BMI270_DEFAULT_CONFIG(s_i2c_bus);
    const bmi270_error_t sensor_result = bmi270_open(&sensor_config, &s_bmi270);
    if (sensor_result != BMI270_OK) {
        ESP_LOGE(TAG, "BMI270 initialization failed: %d", sensor_result);
        cleanup_sensor_bus();
        return;
    }

    const bmi270_error_t anymotion_result =
        bmi270_enable_anymotion_interrupt(s_bmi270, s_power, BMI270_ANYMOTION_THRESHOLD_DEFAULT);
    if (anymotion_result != BMI270_OK) {
        ESP_LOGE(TAG, "BMI270 any-motion setup failed: %d", anymotion_result);
        cleanup_sensor_bus();
        return;
    }

    session_controller_error_t session_result =
        session_controller_initialize(s_power, &s_session_controller);
    if (session_result == SESSION_CONTROLLER_OK) {
        session_result = session_controller_start(s_session_controller);
    }
    if (session_result != SESSION_CONTROLLER_OK) {
        ESP_LOGE(TAG, "Session controller setup failed: %d", session_result);
        cleanup_sensor_bus();
        return;
    }

    shutdown_button_error_t shutdown_button_result =
        shutdown_button_initialize(s_session_controller, &s_shutdown_button);
    if (shutdown_button_result == SHUTDOWN_BUTTON_OK) {
        shutdown_button_result = shutdown_button_start(s_shutdown_button);
    }
    if (shutdown_button_result != SHUTDOWN_BUTTON_OK) {
        ESP_LOGE(TAG, "KEY2 shutdown listener setup failed: %d", shutdown_button_result);
        cleanup_sensor_bus();
        return;
    }

    QueueHandle_t button_press_queue;
    capture_button_error_t capture_button_result =
        capture_button_initialize(&s_capture_button, &button_press_queue);
    if (capture_button_result == CAPTURE_BUTTON_OK) {
        capture_button_result = capture_button_start(s_capture_button);
    }
    if (capture_button_result != CAPTURE_BUTTON_OK) {
        ESP_LOGE(TAG, "KEY1 capture listener setup failed: %d", capture_button_result);
        cleanup_sensor_bus();
        return;
    }

    imu_buffer_pool_t pool;
    data_recorder_error_t recorder_result = data_recorder_initialize(&pool);
    if (recorder_result != DATA_RECORDER_OK) {
        ESP_LOGE(TAG, "Recorder initialization failed: %d", recorder_result);
        cleanup_sensor_bus();
        return;
    }

    imu_pipeline_context_t *pipeline;
    imu_pipeline_error_t pipeline_result = imu_pipeline_initialize(&pool, &pipeline);
    if (pipeline_result != IMU_PIPELINE_OK) {
        ESP_LOGE(TAG, "IMU pipeline initialization failed: %d", pipeline_result);
        cleanup_sensor_bus();
        return;
    }

    uploader_context_t *uploader;
    uploader_error_t uploader_result =
        uploader_initialize(&pool, s_power, s_session_controller, &uploader);
    if (uploader_result != UPLOADER_OK) {
        ESP_LOGE(TAG, "Uploader initialization failed: %d", uploader_result);
        cleanup_sensor_bus();
        return;
    }
    uploader_result = uploader_start(uploader);
    if (uploader_result != UPLOADER_OK) {
        ESP_LOGE(TAG, "Uploader startup failed: %d", uploader_result);
        cleanup_sensor_bus();
        return;
    }
    pipeline_result = imu_pipeline_start(pipeline);
    if (pipeline_result != IMU_PIPELINE_OK) {
        ESP_LOGE(TAG, "IMU pipeline startup failed: %d", pipeline_result);
        cleanup_sensor_bus();
        return;
    }

    stop_boot_led();
    if (!start_normal_led()) {
        cleanup_sensor_bus();
        return;
    }
    recorder_result = data_recorder_start(s_bmi270, &pool, s_session_controller, button_press_queue);
    if (recorder_result != DATA_RECORDER_OK) {
        ESP_LOGE(TAG, "Recorder startup failed: %d", recorder_result);
        cleanup_sensor_bus();
        return;
    }
}
