#include "data_recorder.h"

#include <stdbool.h>
#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "idle_detector.h"

static const char *TAG = "recorder";
enum {
    WINDOW_DURATION_SECONDS = 30U,
    WINDOW_MAX_SAMPLES = 3000U,
    BUFFER_COUNT = 2U,
    SCRATCH_CAPACITY = 32U,
    IDLE_MAX_AXIS_DELTA_LSB = 40U,
    IDLE_DURATION_SECONDS = 15U,
};
static const TickType_t READ_RETRY_TICKS = pdMS_TO_TICKS(100);
static const TickType_t BACKPRESSURE_WAIT_TICKS = pdMS_TO_TICKS(1000);

typedef struct recorder_context {
    bmi270_handle_t *sensor;
    imu_buffer_pool_t pool;
    session_controller_t *session;
    idle_detector_t idle_detector;
    size_t window_capacity;
    uint32_t sample_rate_hz;
    bool initialized;
    bool started;
} recorder_context_t;

static acceleration_sample_t s_window_storage[BUFFER_COUNT][WINDOW_MAX_SAMPLES];
static recorder_context_t s_context;

static void return_window_to_free_queue(const recorder_context_t *context, const imu_window_t *window)
{
    while (xQueueSend(context->pool.free_queue, window, BACKPRESSURE_WAIT_TICKS) != pdPASS) {
        ESP_LOGE(TAG, "Could not return discarded window to free queue; retrying");
    }
}

static void recorder_task(void *argument)
{
    recorder_context_t *context = argument;
    bmi270_data_t scratch[SCRATCH_CAPACITY];

    for (;;) {
        imu_window_t window;
        while (xQueueReceive(context->pool.free_queue, &window, BACKPRESSURE_WAIT_TICKS) != pdPASS) {
            ESP_LOGW(TAG, "Backpressure: all acquisition buffers are in the processing pipeline");
        }

        window.count = 0U;
        window.capacity = context->window_capacity;
        window.sample_rate_hz = context->sample_rate_hz;
        window.start_time = (time_t)-1;
        window.end_time = (time_t)-1;
        window.shutdown_after_upload = false;

        window.start_time = time(NULL);
        if (window.start_time == (time_t)-1) {
            ESP_LOGE(TAG, "Could not read capture start time; discarding window");
            return_window_to_free_queue(context, &window);
            continue;
        }

        while (window.count < window.capacity) {
            const size_t remaining = window.capacity - window.count;
            const size_t request_capacity = remaining < SCRATCH_CAPACITY ? remaining : SCRATCH_CAPACITY;
            size_t samples_read = 0U;
            const bmi270_error_t read_result =
                bmi270_read(context->sensor, scratch, request_capacity, &samples_read, 0U);
            if (read_result != BMI270_OK) {
                ESP_LOGE(TAG, "BMI270 read failed: %d; retrying", read_result);
                vTaskDelay(READ_RETRY_TICKS);
                continue;
            }
            if (samples_read > request_capacity) {
                ESP_LOGE(TAG, "BMI270 returned invalid sample count %u (requested %u)",
                         (unsigned)samples_read, (unsigned)request_capacity);
                vTaskDelay(READ_RETRY_TICKS);
                continue;
            }

            for (size_t index = 0; index < samples_read && window.count < window.capacity; ++index) {
                if ((scratch[index].flags & BMI270_DATA_ACCEL_VALID) == 0U) {
                    continue;
                }
                const acceleration_sample_t sample = {
                    .sensor_time = scratch[index].sensor_time,
                    .accel_x = scratch[index].accel_x,
                    .accel_y = scratch[index].accel_y,
                    .accel_z = scratch[index].accel_z,
                };
                window.samples[window.count++] = sample;

                bool state_changed = false;
                const idle_detector_error_t idle_result =
                    idle_detector_update(&context->idle_detector, &sample, &state_changed);
                if (idle_result != IDLE_DETECTOR_OK) {
                    ESP_LOGE(TAG, "Idle detector update failed: %d", idle_result);
                } else if (state_changed && context->idle_detector.state == IDLE_DETECTOR_IDLE &&
                           !window.shutdown_after_upload) {
                    ESP_LOGI(TAG, "Idle detected; shutdown pending at capture boundary");
                    window.shutdown_after_upload = true;
                    if (session_controller_request_idle_shutdown(context->session) != SESSION_CONTROLLER_OK) {
                        ESP_LOGE(TAG, "Could not request idle shutdown");
                        window.shutdown_after_upload = false;
                    }
                } else if (state_changed && context->idle_detector.state == IDLE_DETECTOR_ACTIVE &&
                           window.shutdown_after_upload) {
                    if (session_controller_cancel_idle_shutdown(context->session) != SESSION_CONTROLLER_OK) {
                        ESP_LOGE(TAG, "Could not cancel idle shutdown after motion resumed");
                    } else {
                        ESP_LOGI(TAG, "Motion resumed; idle shutdown cancelled");
                        window.shutdown_after_upload = false;
                    }
                }
            }
        }

        window.end_time = time(NULL);
        if (window.end_time == (time_t)-1) {
            ESP_LOGE(TAG, "Could not read capture end time; discarding window");
            return_window_to_free_queue(context, &window);
            continue;
        }

        if (xQueueSend(context->pool.pipeline_queue, &window, portMAX_DELAY) != pdPASS) {
            ESP_LOGE(TAG, "Fatal ownership invariant violation: could not hand off completed window");
            while (xQueueSend(context->pool.pipeline_queue, &window, BACKPRESSURE_WAIT_TICKS) != pdPASS) {
                ESP_LOGE(TAG, "Completed window is still waiting for pipeline ownership transfer");
            }
        }
        if (window.shutdown_after_upload) {
            if (session_controller_notify_final_window_queued(context->session) != SESSION_CONTROLLER_OK) {
                ESP_LOGE(TAG, "Could not report final window handoff");
            }
            ESP_LOGI(TAG, "Final idle window handed to pipeline; recorder stopping");
            vTaskDelete(NULL);
        }
    }
}

data_recorder_error_t data_recorder_initialize(imu_buffer_pool_t *pool)
{
    if (pool == NULL) {
        ESP_LOGE(TAG, "Initialize failed: pool is null");
        return DATA_RECORDER_ERR_INVALID_ARGUMENT;
    }
    if (s_context.initialized) {
        ESP_LOGE(TAG, "Initialize failed: recorder is already initialized");
        return DATA_RECORDER_ERR_INVALID_STATE;
    }

    QueueHandle_t free_queue = xQueueCreate(BUFFER_COUNT, sizeof(imu_window_t));
    QueueHandle_t pipeline_queue = xQueueCreate(BUFFER_COUNT, sizeof(imu_window_t));
    QueueHandle_t upload_queue = xQueueCreate(BUFFER_COUNT, sizeof(imu_window_t));
    if (free_queue == NULL || pipeline_queue == NULL || upload_queue == NULL) {
        ESP_LOGE(TAG, "Initialize failed: could not create ownership queues");
        if (free_queue != NULL) {
            vQueueDelete(free_queue);
        }
        if (pipeline_queue != NULL) vQueueDelete(pipeline_queue);
        if (upload_queue != NULL) {
            vQueueDelete(upload_queue);
        }
        return DATA_RECORDER_ERR_QUEUE_CREATE_FAILED;
    }

    for (size_t index = 0; index < BUFFER_COUNT; ++index) {
        const imu_window_t window = {
            .samples = s_window_storage[index],
            .count = 0U,
            .capacity = WINDOW_MAX_SAMPLES,
            .sample_rate_hz = 0U,
            .start_time = (time_t)-1,
            .end_time = (time_t)-1,
            .shutdown_after_upload = false,
        };
        if (xQueueSend(free_queue, &window, 0U) != pdPASS) {
            ESP_LOGE(TAG, "Initialize failed: could not seed free buffer queue");
            vQueueDelete(free_queue);
            vQueueDelete(pipeline_queue);
            vQueueDelete(upload_queue);
            return DATA_RECORDER_ERR_QUEUE_SEED_FAILED;
        }
    }

    s_context.pool = (imu_buffer_pool_t){
        .free_queue = free_queue,
        .pipeline_queue = pipeline_queue,
        .upload_queue = upload_queue,
    };
    s_context.initialized = true;
    *pool = s_context.pool;
    ESP_LOGI(TAG, "Initialized two static acquisition buffers");
    return DATA_RECORDER_OK;
}

data_recorder_error_t data_recorder_start(bmi270_handle_t *sensor, const imu_buffer_pool_t *pool,
                                          session_controller_t *session)
{
    if (sensor == NULL || pool == NULL || session == NULL || pool->free_queue == NULL ||
        pool->pipeline_queue == NULL || pool->upload_queue == NULL) {
        ESP_LOGE(TAG, "Start failed: invalid sensor, pool, or session");
        return DATA_RECORDER_ERR_INVALID_ARGUMENT;
    }
    if (!s_context.initialized || s_context.started || pool->free_queue != s_context.pool.free_queue ||
        pool->pipeline_queue != s_context.pool.pipeline_queue || pool->upload_queue != s_context.pool.upload_queue) {
        ESP_LOGE(TAG, "Start failed: recorder is not ready");
        return DATA_RECORDER_ERR_INVALID_STATE;
    }

    const uint32_t sample_rate_hz = bmi270_get_sample_rate_hz(sensor);
    if (sample_rate_hz == 0U || sample_rate_hz > WINDOW_MAX_SAMPLES / WINDOW_DURATION_SECONDS) {
        ESP_LOGE(TAG, "Start failed: unsupported sample rate %uHz", (unsigned)sample_rate_hz);
        return DATA_RECORDER_ERR_UNSUPPORTED_SAMPLE_RATE;
    }

    s_context.sensor = sensor;
    s_context.window_capacity = (size_t)sample_rate_hz * WINDOW_DURATION_SECONDS;
    s_context.sample_rate_hz = sample_rate_hz;
    s_context.session = session;
    const idle_detector_error_t idle_result =
        idle_detector_initialize(&s_context.idle_detector, IDLE_MAX_AXIS_DELTA_LSB,
                                 sample_rate_hz * IDLE_DURATION_SECONDS);
    if (idle_result != IDLE_DETECTOR_OK) {
        ESP_LOGE(TAG, "Start failed: idle detector initialization failed: %d", idle_result);
        return DATA_RECORDER_ERR_IDLE_DETECTOR;
    }
    const BaseType_t result = xTaskCreate(recorder_task, "recorder", 4096U, &s_context, 5U, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Start failed: could not create task");
        return DATA_RECORDER_ERR_TASK_CREATE_FAILED;
    }

    s_context.started = true;
    ESP_LOGI(TAG, "Recorder task started: %u samples per window", (unsigned)s_context.window_capacity);
    return DATA_RECORDER_OK;
}
