idf_component_register(SRCS "storage_service.c"
                       INCLUDE_DIRS "include"
                       REQUIRES board_config fatfs sdmmc driver app_common)
