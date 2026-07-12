#include "config.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "config";
static const char *NAMESPACE = "credentials";

static config_err_t load_string(nvs_handle_t handle, const char *key, char *value, size_t value_size)
{
    size_t required_size = 0;
    esp_err_t err = nvs_get_str(handle, key, NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Missing required credential: %s", key);
        return CONFIG_ERR_MISSING_VALUE;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not read credential length for %s: %s", key, esp_err_to_name(err));
        return CONFIG_ERR_INVALID_VALUE;
    }
    if (required_size <= 1 || required_size > value_size) {
        ESP_LOGE(TAG, "Credential %s has an invalid length", key);
        return CONFIG_ERR_INVALID_LENGTH;
    }

    err = nvs_get_str(handle, key, value, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not read credential %s: %s", key, esp_err_to_name(err));
        return CONFIG_ERR_INVALID_VALUE;
    }
    return CONFIG_OK;
}

config_err_t config_load_credentials(config_credentials_t *credentials)
{
    if (credentials == NULL) {
        ESP_LOGE(TAG, "Credentials output is null");
        return CONFIG_ERR_INVALID_VALUE;
    }

    memset(credentials, 0, sizeof(*credentials));

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not open credentials namespace: %s", esp_err_to_name(err));
        return CONFIG_ERR_OPEN;
    }

    config_err_t result = load_string(handle, "ssid", credentials->ssid, sizeof(credentials->ssid));
    if (result == CONFIG_OK) {
        result = load_string(handle, "password", credentials->password, sizeof(credentials->password));
    }
    if (result == CONFIG_OK) {
        result = load_string(handle, "device_id", credentials->device_id, sizeof(credentials->device_id));
    }
    if (result == CONFIG_OK) {
        result = load_string(handle, "secret_key", credentials->secret_key, sizeof(credentials->secret_key));
    }
    if (result == CONFIG_OK) {
        result = load_string(handle, "lambda_url", credentials->lambda_url, sizeof(credentials->lambda_url));
    }

    nvs_close(handle);
    return result;
}
