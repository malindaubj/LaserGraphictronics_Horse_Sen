#include "sensor_service.h"

#include "bmi270.h"
#include "board_config.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "max30101.h"

static const char *TAG = "sensor_service";
static sensor_health_t s_health;
static max30101_t s_ppg_1 = {.mux_channel = BOARD_MUX_CHANNEL_PPG_1};
static max30101_t s_ppg_2 = {.mux_channel = BOARD_MUX_CHANNEL_PPG_2};
static uint32_t s_sequence;

esp_err_t sensor_service_init(void) {
    s_health.ppg_1_ok = max30101_probe(&s_ppg_1) == ESP_OK;
    s_health.ppg_2_ok = max30101_probe(&s_ppg_2) == ESP_OK;
    s_health.accelerometer_ok = bmi270_probe(&s_health.accelerometer_ok) == ESP_OK;
    return (s_health.ppg_1_ok && s_health.ppg_2_ok) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t sensor_service_start(void) {
    s_sequence = 0;
    ESP_RETURN_ON_ERROR(max30101_start(&s_ppg_1), TAG, "PPG 1 start failed");
    return max30101_start(&s_ppg_2);
}

esp_err_t sensor_service_stop(void) {
    esp_err_t first = max30101_stop(&s_ppg_1);
    esp_err_t second = max30101_stop(&s_ppg_2);
    return first != ESP_OK ? first : second;
}

esp_err_t sensor_service_read_batch(app_sample_batch_t *batch) {
    if (batch == NULL)
        return ESP_ERR_INVALID_ARG;
    batch->count = 0;
    const int64_t deadline_us = esp_timer_get_time() + 100000;
    while (batch->count < APP_SAMPLE_BATCH_SIZE && esp_timer_get_time() < deadline_us) {
        bool first_available = false;
        bool second_available = false;
        ESP_RETURN_ON_ERROR(max30101_sample_available(&s_ppg_1, &first_available), TAG,
                            "PPG 1 availability failed");
        ESP_RETURN_ON_ERROR(max30101_sample_available(&s_ppg_2, &second_available), TAG,
                            "PPG 2 availability failed");
        if (!first_available || !second_available) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        app_ppg_sample_t *sample = &batch->samples[batch->count];
        ESP_RETURN_ON_ERROR(max30101_read_sample(&s_ppg_1, &sample->ppg_1), TAG,
                            "PPG 1 read failed");
        ESP_RETURN_ON_ERROR(max30101_read_sample(&s_ppg_2, &sample->ppg_2), TAG,
                            "PPG 2 read failed");
        sample->timestamp_us = (uint64_t)esp_timer_get_time();
        sample->sequence = s_sequence++;
        batch->count++;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(max30101_update_overflow_count(&s_ppg_1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(max30101_update_overflow_count(&s_ppg_2));
    return batch->count > 0U ? ESP_OK : ESP_ERR_TIMEOUT;
}

sensor_health_t sensor_service_health(void) {
    return s_health;
}
