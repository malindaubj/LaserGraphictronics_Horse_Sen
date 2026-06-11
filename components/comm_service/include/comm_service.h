#pragma once

#include "app_types.h"
#include "esp_err.h"
#include "freertos/queue.h"

esp_err_t comm_service_init(QueueHandle_t command_queue);
void comm_service_publish_status(const app_status_t *status);
