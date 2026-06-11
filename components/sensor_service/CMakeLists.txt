idf_component_register(SRCS "sensor_service.c"
                       INCLUDE_DIRS "include"
                       REQUIRES board_config max30101 bmi270 app_common esp_timer)
