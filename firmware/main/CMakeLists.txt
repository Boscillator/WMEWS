idf_component_register(SRCS "hello_world_main.c" "config.c" "network.c" "power.c" "bmi270.c" "capture_button.c" "shutdown_button.c" "session_controller.c" "idle_detector.c" "feature_pipeline.c" "imu_pipeline.c" "data_recorder.c" "capture_metadata.c" "uploader.c" "uploader_json.c" "uploader_http.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_driver_i2c esp_driver_gpio
                       PRIV_REQUIRES nvs_flash esp_wifi esp_event esp_netif freertos espressif__cjson app_update esp_http_client mbedtls esp_timer esp_hw_support heap)

nvs_create_partition_image(nvs ../config.csv FLASH_IN_PROJECT)
