idf_component_register(SRCS "hello_world_main.c" "config.c" "network.c" "bmi270.c" "data_recorder.c" "uploader.c" "uploader_json.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash esp_wifi esp_event esp_netif freertos espressif__cjson app_update)

nvs_create_partition_image(nvs ../config.csv FLASH_IN_PROJECT)
