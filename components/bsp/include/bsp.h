#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BSP_LED_RED,
    BSP_LED_GREEN,
    BSP_LED_BLUE,
    BSP_LED_COUNT,
} bsp_led_t;

esp_err_t bsp_init(void);
esp_err_t bsp_i2c_mux_select(uint8_t mux_channel);
esp_err_t bsp_i2c_read(uint8_t address, uint8_t reg, void *data, size_t length);
esp_err_t bsp_i2c_write(uint8_t address, uint8_t reg, const void *data, size_t length);
void bsp_led_set(bsp_led_t led, bool on);
void bsp_sensor_power_set(bool on);
uint8_t bsp_battery_percent(void);
