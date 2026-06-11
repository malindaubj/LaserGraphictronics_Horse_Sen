#pragma once

#include "app_types.h"
#include "esp_err.h"
#include <stdbool.h>

esp_err_t storage_service_init(void);
esp_err_t storage_service_begin_session(const char *horse_name);
esp_err_t storage_service_write_batch(const app_sample_batch_t *batch);
esp_err_t storage_service_end_session(void);
bool storage_service_is_ready(void);
