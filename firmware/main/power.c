#include "power.h"

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "power";

enum {
    REG_DEVICE_ID = 0x00,
    REG_WAKE_SRC = 0x05,
    REG_HOLD_CFG = 0x07,
    REG_SYS_CMD = 0x0C,
    REG_GPIO_MODE = 0x10,
    REG_GPIO_PU_PD_1 = 0x15,
    REG_GPIO_FUNC_1 = 0x17,
    REG_GPIO_WAKE_EN = 0x18,
    REG_GPIO_WAKE_CFG = 0x19,
    REG_VBAT_L = 0x22,
    PMIC_DEVICE_ID = 0x50,
    PMIC_DEVICE_MODEL = 0x20,
    PMIC_GPIO4_BIT = UINT8_C(1) << 4,
    PMIC_GPIO4_PULL_MASK = UINT8_C(0x03),
    PMIC_SYS_CMD_POWEROFF = 0xA1,
    I2C_TIMEOUT_MS = 100,
};

typedef enum {
    STATE_CLOSED,
    STATE_READY,
} power_state_t;

struct power_handle {
    i2c_master_dev_handle_t device;
    SemaphoreHandle_t lock;
    power_state_t state;
};

static power_handle_t s_handle;
static StaticSemaphore_t s_lock_storage;

static void reset_handle(void)
{
    memset(&s_handle, 0, sizeof(s_handle));
    s_handle.lock = xSemaphoreCreateMutexStatic(&s_lock_storage);
    s_handle.state = STATE_CLOSED;
}

static bool is_ready(const power_handle_t *handle)
{
    return handle == &s_handle && handle->state == STATE_READY && handle->device != NULL;
}

static power_error_t transport_result(esp_err_t err, const char *operation, uint8_t reg)
{
    if (err == ESP_OK) return POWER_OK;
    ESP_LOGE(TAG, "%s reg 0x%02" PRIx8 " failed: %s", operation, reg, esp_err_to_name(err));
    return POWER_ERR_TRANSPORT;
}

static power_error_t read_register(uint8_t reg, uint8_t *data, size_t length)
{
    return transport_result(i2c_master_transmit_receive(s_handle.device, &reg, 1, data, length,
                                                         I2C_TIMEOUT_MS),
                            "read", reg);
}

static power_error_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return transport_result(i2c_master_transmit(s_handle.device, data, sizeof(data), I2C_TIMEOUT_MS),
                            "write", reg);
}

static power_error_t update_register(uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current;
    power_error_t result = read_register(reg, &current, sizeof(current));
    if (result != POWER_OK) return result;
    const uint8_t next = (uint8_t)((current & (uint8_t)~mask) | (value & mask));
    return next == current ? POWER_OK : write_register(reg, next);
}

static power_error_t verify_register_bits(uint8_t reg, uint8_t mask, uint8_t expected)
{
    uint8_t actual;
    power_error_t result = read_register(reg, &actual, sizeof(actual));
    if (result != POWER_OK) return result;
    if ((actual & mask) == (expected & mask)) return POWER_OK;
    ESP_LOGE(TAG, "configuration verify reg 0x%02" PRIx8 " expected 0x%02" PRIx8
                  " mask 0x%02" PRIx8 " got 0x%02" PRIx8,
             reg, expected, mask, actual);
    return POWER_ERR_CONFIGURATION;
}

static void lock(void)
{
    (void)xSemaphoreTake(s_handle.lock, portMAX_DELAY);
}

static void unlock(void)
{
    (void)xSemaphoreGive(s_handle.lock);
}

static power_error_t validate_handle(const power_handle_t *handle, const char *operation)
{
    if (is_ready(handle)) return POWER_OK;
    ESP_LOGE(TAG, "%s in invalid state", operation);
    return POWER_ERR_INVALID_STATE;
}

power_error_t power_open(const power_config_t *config, power_handle_t **handle)
{
    if (handle == NULL) {
        ESP_LOGE(TAG, "open output is null");
        return POWER_ERR_INVALID_ARGUMENT;
    }
    *handle = NULL;
    if (config == NULL || config->bus == NULL || config->i2c_address > 0x7FU ||
        config->i2c_clock_hz == 0U) {
        ESP_LOGE(TAG, "invalid PMIC configuration");
        return POWER_ERR_INVALID_CONFIG;
    }
    if (s_handle.state != STATE_CLOSED) {
        ESP_LOGE(TAG, "open in invalid state");
        return POWER_ERR_INVALID_STATE;
    }

    reset_handle();
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->i2c_address,
        .scl_speed_hz = config->i2c_clock_hz,
    };
    const esp_err_t err = i2c_master_bus_add_device(config->bus, &device_config, &s_handle.device);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err));
        reset_handle();
        return POWER_ERR_TRANSPORT;
    }

    uint8_t identity[4];
    power_error_t result = read_register(REG_DEVICE_ID, identity, sizeof(identity));
    if (result == POWER_OK &&
        (identity[0] != PMIC_DEVICE_ID || identity[1] != PMIC_DEVICE_MODEL)) {
        ESP_LOGE(TAG, "unexpected PMIC identity: device=0x%02" PRIx8 " model=0x%02" PRIx8,
                 identity[0], identity[1]);
        result = POWER_ERR_DEVICE_ID;
    }
    if (result != POWER_OK) {
        (void)i2c_master_bus_rm_device(s_handle.device);
        reset_handle();
        return result;
    }

    s_handle.state = STATE_READY;
    *handle = &s_handle;
    ESP_LOGI(TAG, "M5PM1 ready: device=0x%02" PRIx8 " model=0x%02" PRIx8
                  " hw=%" PRIu8 " sw=%" PRIu8,
             identity[0], identity[1], identity[2], identity[3]);
    return POWER_OK;
}

power_error_t power_read_battery_mv(power_handle_t *handle, uint16_t *millivolts)
{
    if (millivolts == NULL) {
        ESP_LOGE(TAG, "battery output is null");
        return POWER_ERR_INVALID_ARGUMENT;
    }
    *millivolts = 0;
    power_error_t result = validate_handle(handle, "battery read");
    if (result != POWER_OK) return result;

    lock();
    uint8_t bytes[2];
    result = read_register(REG_VBAT_L, bytes, sizeof(bytes));
    unlock();
    if (result != POWER_OK) return result;

    *millivolts = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    ESP_LOGD(TAG, "battery voltage: %" PRIu16 " mV", *millivolts);
    return POWER_OK;
}

power_error_t power_get_wake_sources(power_handle_t *handle, power_wake_flags_t *sources)
{
    if (sources == NULL) {
        ESP_LOGE(TAG, "wake-source output is null");
        return POWER_ERR_INVALID_ARGUMENT;
    }
    *sources = 0;
    power_error_t result = validate_handle(handle, "wake-source read");
    if (result != POWER_OK) return result;

    lock();
    result = read_register(REG_WAKE_SRC, sources, sizeof(*sources));
    unlock();
    if (result == POWER_OK) ESP_LOGI(TAG, "wake sources: 0x%02" PRIx8, *sources);
    return result;
}

power_error_t power_clear_wake_sources(power_handle_t *handle, power_wake_flags_t sources)
{
    power_error_t result = validate_handle(handle, "wake-source clear");
    if (result != POWER_OK) return result;
    if (sources == 0U) return POWER_OK;

    lock();
    result = write_register(REG_WAKE_SRC, sources);
    unlock();
    if (result == POWER_OK) ESP_LOGD(TAG, "cleared wake sources: 0x%02" PRIx8, sources);
    return result;
}

power_error_t power_configure_motion_wake(power_handle_t *handle, power_wake_edge_t edge,
                                           power_wake_pull_t pull)
{
    if (edge != POWER_WAKE_EDGE_FALLING && edge != POWER_WAKE_EDGE_RISING) {
        ESP_LOGE(TAG, "invalid motion wake edge");
        return POWER_ERR_INVALID_ARGUMENT;
    }
    if (pull != POWER_WAKE_PULL_NONE && pull != POWER_WAKE_PULL_UP &&
        pull != POWER_WAKE_PULL_DOWN) {
        ESP_LOGE(TAG, "invalid motion wake pull");
        return POWER_ERR_INVALID_ARGUMENT;
    }
    power_error_t result = validate_handle(handle, "motion-wake configuration");
    if (result != POWER_OK) return result;

    const uint8_t pull_value = pull == POWER_WAKE_PULL_UP ? 0x01U :
                               pull == POWER_WAKE_PULL_DOWN ? 0x02U : 0x00U;
    const uint8_t edge_value = edge == POWER_WAKE_EDGE_RISING ? PMIC_GPIO4_BIT : 0U;
    const uint8_t hold_value = pull == POWER_WAKE_PULL_NONE ? 0U : PMIC_GPIO4_BIT;

    /* Disable wake while GPIO4 is reconfigured so a pull or edge change cannot
     * produce a false power-on. Configure the input completely, retain its pull
     * through power-off when needed, clear a stale motion wake, then enable wake
     * last. The read-backs make an I2C or PMIC configuration failure visible. */
    lock();
    result = update_register(REG_GPIO_WAKE_EN, PMIC_GPIO4_BIT, 0U);
    if (result == POWER_OK) result = update_register(REG_GPIO_FUNC_1, PMIC_GPIO4_PULL_MASK, 0U);
    if (result == POWER_OK) result = update_register(REG_GPIO_MODE, PMIC_GPIO4_BIT, 0U);
    if (result == POWER_OK) result = update_register(REG_GPIO_PU_PD_1, PMIC_GPIO4_PULL_MASK, pull_value);
    if (result == POWER_OK) result = update_register(REG_HOLD_CFG, PMIC_GPIO4_BIT, hold_value);
    if (result == POWER_OK) result = update_register(REG_GPIO_WAKE_CFG, PMIC_GPIO4_BIT, edge_value);
    if (result == POWER_OK) result = write_register(REG_WAKE_SRC, POWER_WAKE_EXTERNAL);
    if (result == POWER_OK) result = update_register(REG_GPIO_WAKE_EN, PMIC_GPIO4_BIT, PMIC_GPIO4_BIT);
    if (result == POWER_OK) result = verify_register_bits(REG_GPIO_WAKE_EN, PMIC_GPIO4_BIT, PMIC_GPIO4_BIT);
    if (result == POWER_OK) result = verify_register_bits(REG_GPIO_WAKE_CFG, PMIC_GPIO4_BIT, edge_value);
    if (result == POWER_OK) result = verify_register_bits(REG_GPIO_PU_PD_1, PMIC_GPIO4_PULL_MASK, pull_value);
    if (result == POWER_OK) result = verify_register_bits(REG_HOLD_CFG, PMIC_GPIO4_BIT, hold_value);
    unlock();

    if (result == POWER_OK) {
        const char *const edge_name = edge == POWER_WAKE_EDGE_RISING ? "rising" : "falling";
        const char *const pull_name = pull == POWER_WAKE_PULL_UP ? "up" :
                                      pull == POWER_WAKE_PULL_DOWN ? "down" : "none";
        ESP_LOGI(TAG, "motion wake armed: PMIC GPIO4, %s edge, pull=%s, hold=%s", edge_name,
                 pull_name, hold_value != 0U ? "yes" : "no");
    }
    return result;
}

power_error_t power_disable_motion_wake(power_handle_t *handle)
{
    power_error_t result = validate_handle(handle, "motion-wake disable");
    if (result != POWER_OK) return result;

    lock();
    result = update_register(REG_GPIO_WAKE_EN, PMIC_GPIO4_BIT, 0U);
    if (result == POWER_OK) result = update_register(REG_GPIO_PU_PD_1, PMIC_GPIO4_PULL_MASK, 0U);
    if (result == POWER_OK) result = update_register(REG_HOLD_CFG, PMIC_GPIO4_BIT, 0U);
    unlock();
    if (result == POWER_OK) ESP_LOGI(TAG, "motion wake disabled: PMIC GPIO4");
    return result;
}

power_error_t power_off(power_handle_t *handle)
{
    power_error_t result = validate_handle(handle, "power-off");
    if (result != POWER_OK) return result;

    ESP_LOGW(TAG, "entering PMIC power-off; waiting for configured wake source");
    lock();
    uint8_t wake_enable;
    result = read_register(REG_GPIO_WAKE_EN, &wake_enable, sizeof(wake_enable));
    if (result == POWER_OK && (wake_enable & PMIC_GPIO4_BIT) == 0U) {
        ESP_LOGE(TAG, "refusing power-off: PMIC GPIO4 motion wake is not armed");
        result = POWER_ERR_CONFIGURATION;
    }
    if (result == POWER_OK) result = write_register(REG_SYS_CMD, PMIC_SYS_CMD_POWEROFF);
    unlock();
    if (result != POWER_OK) return result;

    for (;;) vTaskDelay(portMAX_DELAY);
}

power_error_t power_close(power_handle_t *handle)
{
    power_error_t result = validate_handle(handle, "close");
    if (result != POWER_OK) return result;

    lock();
    const esp_err_t err = i2c_master_bus_rm_device(s_handle.device);
    if (err != ESP_OK) {
        unlock();
        ESP_LOGE(TAG, "remove device failed: %s", esp_err_to_name(err));
        return POWER_ERR_TRANSPORT;
    }
    s_handle.device = NULL;
    s_handle.state = STATE_CLOSED;
    unlock();
    ESP_LOGI(TAG, "M5PM1 closed");
    return POWER_OK;
}
