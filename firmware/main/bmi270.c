#include "bmi270.h"

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bmi270";

enum {
    REG_CHIP_ID = 0x00, REG_SENSORTIME = 0x18, REG_INTERNAL_STATUS = 0x21,
    REG_TEMPERATURE = 0x22, REG_FIFO_LENGTH = 0x24, REG_FIFO_DATA = 0x26,
    REG_ACC_CONF = 0x40, REG_ACC_RANGE = 0x41, REG_GYR_CONF = 0x42,
    REG_GYR_RANGE = 0x43, REG_FIFO_DOWNS = 0x45, REG_FIFO_CONFIG_0 = 0x48,
    REG_FIFO_CONFIG_1 = 0x49, REG_INIT_CTRL = 0x59, REG_INIT_ADDR_0 = 0x5B,
    REG_INIT_DATA = 0x5E, REG_CMD = 0x7E, REG_PWR_CONF = 0x7C, REG_PWR_CTRL = 0x7D,
    CHIP_ID = 0x24, CMD_SOFT_RESET = 0xB6, CMD_FIFO_FLUSH = 0xB0,
    PWR_CTRL_ENABLE_ALL = 0x0E, FIFO_HEADER_ACC_GYR = 0x8C,
    FIFO_CONTROL_SKIP = 0x40, FIFO_CONTROL_TIME = 0x44, FIFO_CONTROL_CONFIG = 0x48,
    FIFO_OVERREAD = 0x80, FIFO_CONFIG_0_TIME = 0x02, FIFO_CONFIG_1_HEADER_ACC_GYR = 0xD0,
    ACC_RANGE_8G = 0x02, GYR_RANGE_2000DPS = 0x00, FIFO_DOWNS_FILTERED = 0x88, CONFIG_IMAGE_SIZE = 8192,
    IMAGE_CHUNK_SIZE = 32, FIFO_BURST_SIZE = 2048, I2C_TIMEOUT_MS = 100,
};

typedef enum { STATE_CLOSED, STATE_INITIALIZING, STATE_READY, STATE_READING, STATE_ERROR } bmi270_state_t;

struct bmi270_handle {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    uint32_t sample_rate_hz;
    uint32_t sensor_ticks_per_sample;
    bmi270_state_t state;
    bool fifo_overrun;
    uint8_t fifo_buffer[FIFO_BURST_SIZE];
};

static bmi270_handle_t s_handle;

/* Bosch Sensortec BMI270 SensorAPI configuration image (BSD-3-Clause, v2.86.1). */
static const uint8_t s_config_image[] = {
#include "bmi270_config_image.inc"
};
_Static_assert(sizeof(s_config_image) == CONFIG_IMAGE_SIZE, "BMI270 configuration image must be 8192 bytes");

static bool is_live(const bmi270_handle_t *handle)
{
    return handle == &s_handle && (handle->state == STATE_READY || handle->state == STATE_READING);
}

static bool odr_code(uint32_t rate, uint8_t *code)
{
    static const struct { uint16_t rate; uint8_t code; } rates[] = {
        {25, 0x06}, {50, 0x07}, {100, 0x08}, {200, 0x09},
        {400, 0x0A}, {800, 0x0B}, {1600, 0x0C},
    };
    for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
        if (rates[i].rate == rate) { *code = rates[i].code; return true; }
    }
    return false;
}

static bmi270_error_t transport_result(esp_err_t err, const char *operation, uint8_t reg)
{
    if (err == ESP_OK) return BMI270_OK;
    ESP_LOGE(TAG, "%s reg 0x%02x failed: %s", operation, reg, esp_err_to_name(err));
    return BMI270_ERR_TRANSPORT;
}

static bmi270_error_t write_reg(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return transport_result(i2c_master_transmit(s_handle.device, data, sizeof(data), I2C_TIMEOUT_MS), "write", reg);
}

static bmi270_error_t write_burst(uint8_t reg, const uint8_t *data, size_t length)
{
    uint8_t buffer[1 + IMAGE_CHUNK_SIZE];
    if (length > IMAGE_CHUNK_SIZE) return BMI270_ERR_INVALID_ARGUMENT;
    buffer[0] = reg;
    memcpy(&buffer[1], data, length);
    return transport_result(i2c_master_transmit(s_handle.device, buffer, length + 1, I2C_TIMEOUT_MS), "burst write", reg);
}

static bmi270_error_t read_reg(uint8_t reg, uint8_t *data, size_t length)
{
    return transport_result(i2c_master_transmit_receive(s_handle.device, &reg, 1, data, length, I2C_TIMEOUT_MS), "read", reg);
}

static bmi270_error_t read_chip_id_after_reset(uint8_t *chip_id)
{
    esp_err_t err = ESP_FAIL;
    for (unsigned attempt = 0; attempt < 20; ++attempt) {
        err = i2c_master_transmit_receive(s_handle.device, (const uint8_t[]){REG_CHIP_ID}, 1,
                                          chip_id, 1, I2C_TIMEOUT_MS);
        if (err == ESP_OK) return BMI270_OK;
        /* A reset aborts I2C transactions; wait for the ASIC to rejoin the bus. */
        vTaskDelay(1);
    }
    return transport_result(err, "read CHIP_ID after reset", REG_CHIP_ID);
}

static int16_t le16(const uint8_t *value) { return (int16_t)((uint16_t)value[0] | ((uint16_t)value[1] << 8)); }
static uint32_t le24(const uint8_t *value) { return (uint32_t)value[0] | ((uint32_t)value[1] << 8) | ((uint32_t)value[2] << 16); }

static void reset_handle(void)
{
    memset(&s_handle, 0, sizeof(s_handle));
    s_handle.state = STATE_CLOSED;
}

static void delay_at_least_ms(uint32_t milliseconds)
{
    const TickType_t ticks = (TickType_t)(((uint64_t)milliseconds * configTICK_RATE_HZ + 999U) / 1000U);
    vTaskDelay(ticks == 0 ? 1 : ticks);
}

static void remove_device(void)
{
    if (s_handle.device != NULL) {
        esp_err_t err = i2c_master_bus_rm_device(s_handle.device);
        if (err != ESP_OK) ESP_LOGE(TAG, "remove device failed: %s", esp_err_to_name(err));
    }
    reset_handle();
}

static bmi270_error_t upload_configuration(void)
{
    bmi270_error_t result = write_reg(REG_PWR_CONF, 0x00);
    if (result != BMI270_OK) return result;
    /* A scheduler tick is safely longer than the required 450 us. */
    vTaskDelay(1);
    if ((result = write_reg(REG_INIT_CTRL, 0x00)) != BMI270_OK) return result;
    for (size_t offset = 0; offset < sizeof(s_config_image); offset += IMAGE_CHUNK_SIZE) {
        /* INIT_ADDR is a 12-bit word address split 4 low bits / 8 high bits;
         * it is not a conventional little-endian 16-bit register value. */
        const uint16_t word_address = (uint16_t)(offset / 2U);
        uint8_t addr[] = {(uint8_t)(word_address & 0x0FU), (uint8_t)(word_address >> 4)};
        if ((result = write_burst(REG_INIT_ADDR_0, addr, sizeof(addr))) != BMI270_OK ||
            (result = write_burst(REG_INIT_DATA, &s_config_image[offset], IMAGE_CHUNK_SIZE)) != BMI270_OK) return result;
    }
    if ((result = write_reg(REG_INIT_CTRL, 0x01)) != BMI270_OK) return result;
    for (unsigned i = 0; i < 30; ++i) {
        uint8_t status;
        vTaskDelay(pdMS_TO_TICKS(10));
        if ((result = read_reg(REG_INTERNAL_STATUS, &status, 1)) != BMI270_OK) return result;
        if ((status & 0x0F) == 0x01) return BMI270_OK;
        if ((status & 0x0F) > 0x01) { ESP_LOGE(TAG, "initialization status 0x%02x", status); return BMI270_ERR_INITIALIZATION; }
    }
    ESP_LOGE(TAG, "initialization status timed out");
    return BMI270_ERR_INITIALIZATION;
}

static bmi270_error_t flush_fifo(void)
{
    bmi270_error_t result = write_reg(REG_CMD, CMD_FIFO_FLUSH);
    if (result == BMI270_OK) s_handle.fifo_overrun = false;
    return result;
}

uint32_t bmi270_get_sample_rate_hz(const bmi270_handle_t *handle) { return is_live(handle) ? handle->sample_rate_hz : 0; }
uint32_t bmi270_get_sensor_time_dt_us(const bmi270_handle_t *handle) { return is_live(handle) ? (handle->sensor_ticks_per_sample * 625U) / 16U : 0; }
float bmi270_get_gyro_lsb(const bmi270_handle_t *handle) { return is_live(handle) ? 16.384F : 0.0F; }
float bmi270_get_accel_lsb(const bmi270_handle_t *handle) { return is_live(handle) ? 4096.0F : 0.0F; }
float bmi270_get_temperature_lsb(const bmi270_handle_t *handle) { return is_live(handle) ? 512.0F : 0.0F; }

bmi270_error_t bmi270_open(const bmi270_config_t *config, bmi270_handle_t **handle)
{
    uint8_t odr;
    if (handle == NULL) { ESP_LOGE(TAG, "open output is null"); return BMI270_ERR_INVALID_ARGUMENT; }
    *handle = NULL;
    if (config == NULL || config->bus == NULL || config->i2c_address > 0x7F ||
        config->i2c_clock_hz == 0 || config->i2c_clock_hz > BMI270_I2C_CLOCK_HZ_DEFAULT ||
        !odr_code(config->sample_rate_hz, &odr)) {
        ESP_LOGE(TAG, "invalid BMI270 configuration"); return BMI270_ERR_INVALID_CONFIG;
    }
    if (s_handle.state != STATE_CLOSED) { ESP_LOGE(TAG, "open in invalid state %d", s_handle.state); return BMI270_ERR_INVALID_STATE; }
    s_handle.state = STATE_INITIALIZING;
    s_handle.bus = config->bus;
    s_handle.sample_rate_hz = config->sample_rate_hz;
    s_handle.sensor_ticks_per_sample = 25600U / config->sample_rate_hz;
    const i2c_device_config_t device_config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7, .device_address = config->i2c_address, .scl_speed_hz = config->i2c_clock_hz};
    esp_err_t err = i2c_master_bus_add_device(config->bus, &device_config, &s_handle.device);
    if (err != ESP_OK) { ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err)); reset_handle(); return BMI270_ERR_TRANSPORT; }
    bmi270_error_t result = write_reg(REG_CMD, CMD_SOFT_RESET);
    if (result == BMI270_OK) delay_at_least_ms(3);
    uint8_t id = 0;
    if (result == BMI270_OK) result = read_chip_id_after_reset(&id);
    if (result == BMI270_OK && id != CHIP_ID) { ESP_LOGE(TAG, "unexpected CHIP_ID 0x%02x", id); result = BMI270_ERR_DEVICE_ID; }
    if (result == BMI270_OK) result = upload_configuration();
    if (result == BMI270_OK) result = write_reg(REG_ACC_CONF, (uint8_t)(0xA0 | odr));
    if (result == BMI270_OK) result = write_reg(REG_GYR_CONF, (uint8_t)(0xA0 | odr));
    if (result == BMI270_OK) result = write_reg(REG_ACC_RANGE, ACC_RANGE_8G);
    if (result == BMI270_OK) result = write_reg(REG_GYR_RANGE, GYR_RANGE_2000DPS);
    if (result == BMI270_OK) result = write_reg(REG_FIFO_DOWNS, FIFO_DOWNS_FILTERED);
    if (result == BMI270_OK) result = write_reg(REG_FIFO_CONFIG_0, FIFO_CONFIG_0_TIME);
    if (result == BMI270_OK) result = write_reg(REG_FIFO_CONFIG_1, FIFO_CONFIG_1_HEADER_ACC_GYR);
    if (result == BMI270_OK) result = write_reg(REG_PWR_CTRL, PWR_CTRL_ENABLE_ALL);
    if (result == BMI270_OK) result = flush_fifo();
    if (result != BMI270_OK) { s_handle.state = STATE_ERROR; remove_device(); return result; }
    delay_at_least_ms(45);
    s_handle.state = STATE_READY;
    *handle = &s_handle;
    ESP_LOGI(TAG, "BMI270 ready: CHIP_ID=0x%02x, ODR=%" PRIu32 "Hz, accel=+-8g gyro=+-2000dps", id, s_handle.sample_rate_hz);
    return BMI270_OK;
}

static bmi270_error_t fifo_error(uint8_t header, size_t index, size_t fifo_length)
{
    ESP_LOGE(TAG, "malformed FIFO data: header=0x%02x index=%u length=%u; flushing",
             header, (unsigned)index, (unsigned)fifo_length);
    (void)flush_fifo();
    return BMI270_ERR_FIFO;
}

bmi270_error_t bmi270_read(bmi270_handle_t *handle, bmi270_data_t *samples, size_t capacity, size_t *samples_read, uint32_t timeout_ms)
{
    if (samples_read == NULL) { ESP_LOGE(TAG, "read count is null"); return BMI270_ERR_INVALID_ARGUMENT; }
    *samples_read = 0;
    if (handle != &s_handle || handle->state != STATE_READY) { ESP_LOGE(TAG, "read in invalid state"); return BMI270_ERR_INVALID_STATE; }
    if (capacity != 0 && samples == NULL) { ESP_LOGE(TAG, "sample buffer is null"); return BMI270_ERR_INVALID_ARGUMENT; }
    if (capacity == 0) return BMI270_OK;
    handle->state = STATE_READING;
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = timeout_ms == 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    size_t count = 0;
    bool overrun = false;
    uint32_t fifo_sensor_time = 0;
    bool have_fifo_sensor_time = false;
    while (count < capacity) {
        uint8_t length_bytes[2];
        bmi270_error_t result = read_reg(REG_FIFO_LENGTH, length_bytes, sizeof(length_bytes));
        if (result != BMI270_OK) { handle->state = STATE_READY; return result; }
        size_t fifo_length = (size_t)length_bytes[0] | ((size_t)(length_bytes[1] & 0x3F) << 8);
        /* A combined frame is 13 bytes. Wait for a full requested batch before
         * consuming FIFO data, while allowing control-frame overhead. */
        if (fifo_length < (capacity - count) * 13U) {
            if (timeout != portMAX_DELAY && (TickType_t)(xTaskGetTickCount() - start) >= timeout) { ESP_LOGE(TAG, "FIFO read timed out"); handle->state = STATE_READY; return BMI270_ERR_TIMEOUT; }
            TickType_t delay = pdMS_TO_TICKS((1000U + handle->sample_rate_hz - 1U) / handle->sample_rate_hz);
            vTaskDelay(delay == 0 ? 1 : delay);
            continue;
        }
        const size_t burst = fifo_length < FIFO_BURST_SIZE ? fifo_length : FIFO_BURST_SIZE;
        result = read_reg(REG_FIFO_DATA, handle->fifo_buffer, burst);
        if (result != BMI270_OK) { handle->state = STATE_READY; return result; }
        const uint8_t *bytes = handle->fifo_buffer;
        for (size_t i = 0; i < burst && count < capacity;) {
            uint8_t header = bytes[i++];
            if (header == FIFO_HEADER_ACC_GYR) {
                if (i + 12 > burst) { handle->state = STATE_READY; return fifo_error(header, i - 1, fifo_length); }
                samples[count] = (bmi270_data_t){.gyro_x = le16(&bytes[i]), .gyro_y = le16(&bytes[i + 2]), .gyro_z = le16(&bytes[i + 4]), .accel_x = le16(&bytes[i + 6]), .accel_y = le16(&bytes[i + 8]), .accel_z = le16(&bytes[i + 10]), .flags = BMI270_DATA_ACCEL_VALID | BMI270_DATA_GYRO_VALID};
                samples[count].flags |= overrun || handle->fifo_overrun ? BMI270_DATA_FIFO_OVERRUN : 0;
                i += 12; ++count;
            } else if (header == FIFO_CONTROL_SKIP) {
                if (i >= burst) { handle->state = STATE_READY; return fifo_error(header, i - 1, fifo_length); }
                overrun = bytes[i++] != 0; handle->fifo_overrun |= overrun;
            } else if (header == FIFO_CONTROL_TIME) {
                if (i + 3 > burst) { handle->state = STATE_READY; return fifo_error(header, i - 1, fifo_length); }
                fifo_sensor_time = le24(&bytes[i]);
                have_fifo_sensor_time = true;
                i += 3;
            } else if (header == FIFO_CONTROL_CONFIG) {
                if (i + 4 > burst) { handle->state = STATE_READY; return fifo_error(header, i - 1, fifo_length); }
                i += 4;
            } else if (header == FIFO_OVERREAD) { break; }
            else { handle->state = STATE_READY; return fifo_error(header, i - 1, fifo_length); }
        }
    }
    uint8_t time_bytes[3], temp_bytes[2];
    bmi270_error_t result = read_reg(REG_SENSORTIME, time_bytes, sizeof(time_bytes));
    if (result == BMI270_OK) result = read_reg(REG_TEMPERATURE, temp_bytes, sizeof(temp_bytes));
    if (result != BMI270_OK) { handle->state = STATE_READY; return result; }
    const uint32_t latest_time = have_fifo_sensor_time ? fifo_sensor_time : le24(time_bytes);
    const int16_t temperature = le16(temp_bytes);
    for (size_t i = 0; i < count; ++i) {
        samples[i].sensor_time = (latest_time - (uint32_t)(count - i - 1U) * handle->sensor_ticks_per_sample) & 0xFFFFFFU;
        samples[i].temperature = temperature;
        if (temperature != INT16_MIN) samples[i].flags |= BMI270_DATA_TEMPERATURE_VALID;
    }
    *samples_read = count;
    handle->fifo_overrun = false;
    handle->state = STATE_READY;
    return BMI270_OK;
}

bmi270_error_t bmi270_close(bmi270_handle_t *handle)
{
    if (handle != &s_handle || (handle->state != STATE_READY && handle->state != STATE_ERROR)) { ESP_LOGE(TAG, "close in invalid state"); return BMI270_ERR_INVALID_STATE; }
    (void)write_reg(REG_FIFO_CONFIG_1, 0x00);
    (void)flush_fifo();
    (void)write_reg(REG_PWR_CTRL, 0x00);
    remove_device();
    ESP_LOGI(TAG, "BMI270 closed");
    return BMI270_OK;
}
