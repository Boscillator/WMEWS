idf_component_register(SRCS "hello_world_main.c" "config.c" "network.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES nvs_flash esp_wifi esp_event esp_netif)

nvs_create_partition_image(nvs ../config.csv FLASH_IN_PROJECT)
