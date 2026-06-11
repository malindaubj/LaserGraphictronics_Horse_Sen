#include "max30101.h"

#include "bsp.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pca9546a.h"

static const char *TAG = "max30101";

enum {
    DEVICE_ADDRESS = 0x57,
    REG_FIFO_WR_PTR = 0x04,
    REG_OVF_COUNTER = 0x05,
    REG_FIFO_RD_PTR = 0x06,
    REG_FIFO_DATA = 0x07,
    REG_FIFO_CONFIG = 0x08,
    REG_MODE_CONFIG = 0x09,
    REG_SPO2_CONFIG = 0x0A,
    REG_LED1_PA = 0x0C,
    REG_LED2_PA = 0x0D,
    REG_PART_ID = 0xFF,
    EXPECTED_PART_ID = 0x15,
};

static esp_err_t select_device(const max30101_t *device) {
    return device == NULL ? ESP_ERR_INVALID_ARG : pca9546a_select(device->mux_channel);
}

static esp_err_t write_u8(uint8_t reg, uint8_t value) {
    return bsp_i2c_write(DEVICE_ADDRESS, reg, &value, sizeof(value));
}

esp_err_t max30101_probe(max30101_t *device) {
    ESP_RETURN_ON_ERROR(select_device(device), TAG, "mux select failed");
    uint8_t part_id = 0;
    ESP_RETURN_ON_ERROR(bsp_i2c_read(DEVICE_ADDRESS, REG_PART_ID, &part_id, 1), TAG,
                        "part ID read failed");
    return part_id == EXPECTED_PART_ID ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t max30101_start(max30101_t *device) {
    ESP_RETURN_ON_ERROR(max30101_probe(device), TAG, "device absent");
    device->last_overflow_value = 0;
    device->fifo_overflow_count = 0;
    ESP_RETURN_ON_ERROR(write_u8(REG_MODE_CONFIG, 0x40), TAG, "reset failed");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_RETURN_ON_ERROR(write_u8(REG_FIFO_WR_PTR, 0), TAG, "FIFO reset failed");
    ESP_RETURN_ON_ERROR(write_u8(REG_OVF_COUNTER, 0), TAG, "overflow reset failed");
    ESP_RETURN_ON_ERROR(write_u8(REG_FIFO_RD_PTR, 0), TAG, "FIFO reset failed");
    /* Four-sample averaging, no rollover, almost-full threshold 15. */
    ESP_RETURN_ON_ERROR(write_u8(REG_FIFO_CONFIG, 0x4F), TAG, "FIFO config failed");
    /* ADC range 8192 nA, 800 samples/s, 411 us pulse width. */
    ESP_RETURN_ON_ERROR(write_u8(REG_SPO2_CONFIG, 0x57), TAG, "sample config failed");
    ESP_RETURN_ON_ERROR(write_u8(REG_LED1_PA, 0x1F), TAG, "LED config failed");
    ESP_RETURN_ON_ERROR(write_u8(REG_LED2_PA, 0x00), TAG, "LED config failed");
    return write_u8(REG_MODE_CONFIG, 0x02);
}

esp_err_t max30101_stop(max30101_t *device) {
    ESP_RETURN_ON_ERROR(select_device(device), TAG, "mux select failed");
    return write_u8(REG_MODE_CONFIG, 0x80);
}

esp_err_t max30101_sample_available(max30101_t *device, bool *available) {
    if (available == NULL)
        return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(select_device(device), TAG, "mux select failed");
    uint8_t write_ptr = 0;
    uint8_t read_ptr = 0;
    ESP_RETURN_ON_ERROR(bsp_i2c_read(DEVICE_ADDRESS, REG_FIFO_WR_PTR, &write_ptr, 1), TAG,
                        "write pointer read failed");
    ESP_RETURN_ON_ERROR(bsp_i2c_read(DEVICE_ADDRESS, REG_FIFO_RD_PTR, &read_ptr, 1), TAG,
                        "read pointer read failed");
    *available = write_ptr != read_ptr;
    return ESP_OK;
}

esp_err_t max30101_read_sample(max30101_t *device, uint32_t *sample) {
    if (sample == NULL)
        return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(select_device(device), TAG, "mux select failed");
    uint8_t bytes[3];
    ESP_RETURN_ON_ERROR(bsp_i2c_read(DEVICE_ADDRESS, REG_FIFO_DATA, bytes, sizeof(bytes)), TAG,
                        "FIFO read failed");
    *sample = (((uint32_t)bytes[0] << 16U) | ((uint32_t)bytes[1] << 8U) | bytes[2]) & 0x3FFFFU;
    return ESP_OK;
}

esp_err_t max30101_update_overflow_count(max30101_t *device) {
    ESP_RETURN_ON_ERROR(select_device(device), TAG, "mux select failed");
    uint8_t overflow = 0;
    ESP_RETURN_ON_ERROR(bsp_i2c_read(DEVICE_ADDRESS, REG_OVF_COUNTER, &overflow, 1), TAG,
                        "overflow read failed");
    overflow &= 0x1FU;
    uint8_t delta = (uint8_t)((overflow - device->last_overflow_value) & 0x1FU);
    device->fifo_overflow_count += delta;
    device->last_overflow_value = overflow;
    return ESP_OK;
}
