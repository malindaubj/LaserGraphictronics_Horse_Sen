#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_common.h"
#include "esp_adc/adc_oneshot.h"

/* Review and approve this file against the released PCB schematic. */
#define BOARD_I2C_PORT I2C_NUM_0
#define BOARD_I2C_SDA_GPIO GPIO_NUM_5
#define BOARD_I2C_SCL_GPIO GPIO_NUM_6
#define BOARD_I2C_FREQUENCY_HZ 400000U
#define BOARD_I2C_TIMEOUT_MS 50U

#define BOARD_SENSOR_ENABLE_GPIO GPIO_NUM_2
#define BOARD_LED_RED_GPIO GPIO_NUM_41
#define BOARD_LED_GREEN_GPIO GPIO_NUM_42
#define BOARD_LED_BLUE_GPIO GPIO_NUM_40
#define BOARD_LED_ACTIVE_LEVEL 1

#define BOARD_BATTERY_ADC_UNIT ADC_UNIT_1
#define BOARD_BATTERY_ADC_CHANNEL ADC_CHANNEL_0
#define BOARD_BATTERY_DIVIDER 2.0f
#define BOARD_BATTERY_MIN_V 3.0f
#define BOARD_BATTERY_MAX_V 4.2f

#define BOARD_I2C_MUX_ADDRESS 0x70U
#define BOARD_MUX_CHANNEL_PPG_1 0U
#define BOARD_MUX_CHANNEL_PPG_2 1U
#define BOARD_MUX_CHANNEL_IMU 2U

/* Provisional until confirmed against the schematic. */
#define BOARD_SD_SPI_HOST SPI2_HOST
#define BOARD_SD_MOSI_GPIO GPIO_NUM_11
#define BOARD_SD_MISO_GPIO GPIO_NUM_13
#define BOARD_SD_SCLK_GPIO GPIO_NUM_12
#define BOARD_SD_CS_GPIO GPIO_NUM_44
