#pragma once

typedef enum {
    NETWORK_OK = 0,
    NETWORK_ERR_CONFIG,
    NETWORK_ERR_EVENT_RESOURCE,
    NETWORK_ERR_EVENT_REGISTRATION,
    NETWORK_ERR_NETIF,
    NETWORK_ERR_WIFI_CONFIGURATION,
    NETWORK_ERR_WIFI_START,
    NETWORK_ERR_CONNECTION_FAILED,
} network_err_t;

/** Configure Wi-Fi station mode and wait for an IPv4 address. */
network_err_t initialize_wifi(void);
