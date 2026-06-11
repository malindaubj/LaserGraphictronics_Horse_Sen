#pragma once

#include "esp_err.h"
#include <stdbool.h>

esp_err_t bmi270_probe(bool *present);
