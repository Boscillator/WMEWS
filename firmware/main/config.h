#pragma once

#include <stddef.h>

#define CONFIG_SSID_MAX_LENGTH 32
#define CONFIG_PASSWORD_MAX_LENGTH 63
#define CONFIG_DEVICE_ID_MAX_LENGTH 63
#define CONFIG_SECRET_KEY_MAX_LENGTH 127

typedef enum {
    CONFIG_OK = 0,
    CONFIG_ERR_OPEN,
    CONFIG_ERR_MISSING_VALUE,
    CONFIG_ERR_INVALID_LENGTH,
    CONFIG_ERR_INVALID_VALUE,
} config_err_t;

/** Credentials loaded from the read-only NVS credentials namespace. */
typedef struct {
    char ssid[CONFIG_SSID_MAX_LENGTH + 1];
    char password[CONFIG_PASSWORD_MAX_LENGTH + 1];
    char device_id[CONFIG_DEVICE_ID_MAX_LENGTH + 1];
    char secret_key[CONFIG_SECRET_KEY_MAX_LENGTH + 1];
} config_credentials_t;

/** Load and validate all required credentials from NVS. */
config_err_t config_load_credentials(config_credentials_t *credentials);
