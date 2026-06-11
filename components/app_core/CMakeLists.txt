idf_component_register(SRCS "app_core.c"
                       INCLUDE_DIRS "include"
                       REQUIRES app_common bsp sensor_service storage_service comm_service
                                nvs_flash esp_timer esp_driver_gptimer)
