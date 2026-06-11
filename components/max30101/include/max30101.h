#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t mux_channel;
    uint8_t last_overflow_value;
    uint32_t fifo_overflow_count;
} max30101_t;

esp_err_t max30101_probe(max30101_t *device);
esp_err_t max30101_start(max30101_t *device);
esp_err_t max30101_stop(max30101_t *device);
esp_err_t max30101_sample_available(max30101_t *device, bool *available);
esp_err_t max30101_read_sample(max30101_t *device, uint32_t *sample);
esp_err_t max30101_update_overflow_count(max30101_t *device);
