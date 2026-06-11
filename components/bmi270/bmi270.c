#include "bmi270.h"

#include "board_config.h"
#include "bsp.h"
#include "esp_check.h"
#include "pca9546a.h"

static const char *TAG = "bmi270";

esp_err_t bmi270_probe(bool *present) {
    if (present == NULL)
        return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(pca9546a_select(BOARD_MUX_CHANNEL_IMU), TAG, "mux select failed");
    uint8_t chip_id = 0;
    ESP_RETURN_ON_ERROR(bsp_i2c_read(0x68U, 0x00U, &chip_id, 1), TAG, "chip ID read failed");
    *present = chip_id == 0x24U;
    return *present ? ESP_OK : ESP_ERR_NOT_FOUND;
}
