#include "network.h"

#include <string.h>

#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT BIT1
#define WIFI_MAXIMUM_RETRIES 5
#define WIFI_CONNECT_TIMEOUT_MS 30000

static const char *TAG = "network";
static EventGroupHandle_t s_event_group;
static int s_retry_count;

static void connect_to_wifi(void)
{
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not begin Wi-Fi connection: %s", esp_err_to_name(err));
        xEventGroupSetBits(s_event_group, WIFI_FAILED_BIT);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGD(TAG, "Wi-Fi station started");
        connect_to_wifi();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < WIFI_MAXIMUM_RETRIES) {
            s_retry_count++;
            ESP_LOGW(TAG, "Wi-Fi disconnected; retrying (%d/%d)", s_retry_count, WIFI_MAXIMUM_RETRIES);
            connect_to_wifi();
        } else {
            ESP_LOGW(TAG, "Wi-Fi connection failed after %d retries", WIFI_MAXIMUM_RETRIES);
            xEventGroupSetBits(s_event_group, WIFI_FAILED_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "Wi-Fi connected; IPv4 address: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0;
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
    }
}

network_err_t initialize_wifi(void)
{
    config_credentials_t credentials;
    if (config_load_credentials(&credentials) != CONFIG_OK) {
        ESP_LOGE(TAG, "Wi-Fi initialization stopped because configuration is invalid");
        return NETWORK_ERR_CONFIG;
    }

    s_event_group = xEventGroupCreate();
    if (s_event_group == NULL) {
        ESP_LOGE(TAG, "Could not create Wi-Fi event group");
        return NETWORK_ERR_EVENT_RESOURCE;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Could not initialize TCP/IP stack: %s", esp_err_to_name(err));
        return NETWORK_ERR_NETIF;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Could not create default event loop: %s", esp_err_to_name(err));
        return NETWORK_ERR_NETIF;
    }

    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(TAG, "Could not create default Wi-Fi station interface");
        return NETWORK_ERR_NETIF;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize Wi-Fi driver: %s", esp_err_to_name(err));
        return NETWORK_ERR_WIFI_CONFIGURATION;
    }

    esp_event_handler_instance_t wifi_handler;
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &wifi_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not register Wi-Fi event handler: %s", esp_err_to_name(err));
        return NETWORK_ERR_EVENT_REGISTRATION;
    }

    esp_event_handler_instance_t ip_handler;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &ip_handler);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not register IP event handler: %s", esp_err_to_name(err));
        return NETWORK_ERR_EVENT_REGISTRATION;
    }

    wifi_config_t wifi_config = { 0 };
    memcpy(wifi_config.sta.ssid, credentials.ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, credentials.password, sizeof(credentials.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) {
        err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not configure Wi-Fi station: %s", esp_err_to_name(err));
        return NETWORK_ERR_WIFI_CONFIGURATION;
    }

    s_retry_count = 0;
    ESP_LOGD(TAG, "Starting Wi-Fi station");
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wi-Fi station: %s", esp_err_to_name(err));
        return NETWORK_ERR_WIFI_START;
    }

    EventBits_t bits = xEventGroupWaitBits(s_event_group, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT, pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    if ((bits & WIFI_CONNECTED_BIT) != 0) {
        return NETWORK_OK;
    }

    ESP_LOGE(TAG, "Wi-Fi did not obtain an IP address");
    return NETWORK_ERR_CONNECTION_FAILED;
}
