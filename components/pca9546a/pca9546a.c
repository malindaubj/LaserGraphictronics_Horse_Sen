#include "pca9546a.h"

#include "bsp.h"

esp_err_t pca9546a_select(uint8_t channel) {
    return bsp_i2c_mux_select(channel);
}
