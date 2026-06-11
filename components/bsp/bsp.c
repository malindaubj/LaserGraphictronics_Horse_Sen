#include "bsp.h"

#include "board_config.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "bsp";

static const gpio_num_t s_led_pins[BSP_LED_COUNT] = {
    [BSP_LED_RED] = BOARD_LED_RED_GPIO,
    [BSP_LED_GREEN] = BOARD_LED_GREEN_GPIO,
    [BSP_LED_BLUE] = BOARD_LED_BLUE_GPIO,
};

static adc_oneshot_unit_handle_t s_adc;

esp_err_t bsp_init(void) {
    gpio_config_t outputs = {
        .pin_bit_mask = BIT64(BOARD_SENSOR_ENABLE_GPIO) | BIT64(BOARD_LED_BLUE_GPIO) |
                        BIT64(BOARD_LED_RED_GPIO) | BIT64(BOARD_LED_GREEN_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&outputs), TAG, "GPIO configuration failed");
    bsp_sensor_power_set(true);
    for (int i = 0; i < BSP_LED_COUNT; ++i) {
        bsp_led_set((bsp_led_t)i, false);
    }

    i2c_config_t bus_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BOARD_I2C_FREQUENCY_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(BOARD_I2C_PORT, &bus_config), TAG, "I2C config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(BOARD_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0), TAG,
                        "I2C init failed");

    adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = BOARD_BATTERY_ADC_UNIT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&adc_config, &s_adc), TAG, "ADC init failed");
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_oneshot_config_channel(s_adc, BOARD_BATTERY_ADC_CHANNEL, &channel_config);
}

esp_err_t bsp_i2c_mux_select(uint8_t mux_channel) {
    if (mux_channel > 3U) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t selection = (uint8_t)(1U << mux_channel);
    return i2c_master_write_to_device(BOARD_I2C_PORT, BOARD_I2C_MUX_ADDRESS, &selection,
                                      sizeof(selection), pdMS_TO_TICKS(BOARD_I2C_TIMEOUT_MS));
}

esp_err_t bsp_i2c_read(uint8_t address, uint8_t reg, void *data, size_t length) {
    if (data == NULL || length == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_write_read_device(BOARD_I2C_PORT, address, &reg, 1, data, length,
                                        pdMS_TO_TICKS(BOARD_I2C_TIMEOUT_MS));
}

esp_err_t bsp_i2c_write(uint8_t address, uint8_t reg, const void *data, size_t length) {
    if (data == NULL || length == 0U || length > 16U) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t frame[17];
    frame[0] = reg;
    for (size_t i = 0; i < length; ++i) {
        frame[i + 1U] = ((const uint8_t *)data)[i];
    }
    return i2c_master_write_to_device(BOARD_I2C_PORT, address, frame, length + 1U,
                                      pdMS_TO_TICKS(BOARD_I2C_TIMEOUT_MS));
}

void bsp_led_set(bsp_led_t led, bool on) {
    if (led < BSP_LED_COUNT) {
        gpio_set_level(s_led_pins[led], on ? BOARD_LED_ACTIVE_LEVEL : !BOARD_LED_ACTIVE_LEVEL);
    }
}

void bsp_sensor_power_set(bool on) {
    gpio_set_level(BOARD_SENSOR_ENABLE_GPIO, on ? 1 : 0);
}

uint8_t bsp_battery_percent(void) {
    int raw = 0;
    if (adc_oneshot_read(s_adc, BOARD_BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
        return 0;
    }
    const float battery_v = ((float)raw / 4095.0f) * 3.3f * BOARD_BATTERY_DIVIDER;
    const float normalized =
        (battery_v - BOARD_BATTERY_MIN_V) / (BOARD_BATTERY_MAX_V - BOARD_BATTERY_MIN_V);
    if (normalized <= 0.0f)
        return 0;
    if (normalized >= 1.0f)
        return 100;
    return (uint8_t)(normalized * 100.0f);
}
