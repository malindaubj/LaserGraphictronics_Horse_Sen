#pragma once

#include "esp_err.h"
#include <stdint.h>

esp_err_t pca9546a_select(uint8_t channel);
