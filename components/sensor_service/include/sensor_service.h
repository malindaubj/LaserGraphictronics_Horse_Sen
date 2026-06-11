#pragma once

#include "app_types.h"
#include "esp_err.h"
#include <stddef.h>

typedef struct {
    bool ppg_1_ok;
    bool ppg_2_ok;
    bool accelerometer_ok;
} sensor_health_t;

esp_err_t sensor_service_init(void);
esp_err_t sensor_service_start(void);
esp_err_t sensor_service_stop(void);
esp_err_t sensor_service_read_batch(app_sample_batch_t *batch);
sensor_health_t sensor_service_health(void);
