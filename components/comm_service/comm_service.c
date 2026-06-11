#include "comm_service.h"

#include <ctype.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "communications";
static QueueHandle_t s_command_queue;

static void submit_command(app_command_id_t id, const char *payload) {
    app_command_t command = {.id = id};
    if (payload != NULL) {
        strlcpy(command.payload, payload, sizeof(command.payload));
    }
    if (xQueueSend(s_command_queue, &command, pdMS_TO_TICKS(50)) != pdPASS) {
        ESP_LOGW(TAG, "Command queue full");
    }
}

static void console_task(void *context) {
    (void)context;
    char line[96];
    size_t used = 0;
    ESP_LOGI(TAG, "Console commands: START, STOP, HORSE <name>, STATUS");
    for (;;) {
        uint8_t byte;
        int read = uart_read_bytes(UART_NUM_0, &byte, 1, pdMS_TO_TICKS(100));
        if (read <= 0) {
            continue;
        }
        if (byte == '\r' || byte == '\n') {
            line[used] = '\0';
            if (strcmp(line, "START") == 0) {
                submit_command(APP_COMMAND_START_RECORDING, NULL);
            } else if (strcmp(line, "STOP") == 0) {
                submit_command(APP_COMMAND_STOP_RECORDING, NULL);
            } else if (strncmp(line, "HORSE ", 6) == 0) {
                submit_command(APP_COMMAND_SET_HORSE, line + 6);
            }
            used = 0;
        } else if (isprint(byte) && used < sizeof(line) - 1U) {
            line[used++] = (char)byte;
        }
    }
}

esp_err_t comm_service_init(QueueHandle_t command_queue) {
    if (command_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_command_queue = command_queue;
    BaseType_t created =
        xTaskCreatePinnedToCore(console_task, "comm_console", 3072, NULL, 3, NULL, 0);
    return created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void comm_service_publish_status(const app_status_t *status) {
    if (status == NULL) {
        return;
    }
    ESP_LOGI(TAG,
             "status recording=%d ppg=%d/%d imu=%d sd=%d battery=%u%% dropped=%lu "
             "sensor_faults=%lu storage_faults=%lu state=%d faults=0x%08lx",
             status->recording, status->ppg_1_ok, status->ppg_2_ok, status->accelerometer_ok,
             status->sd_ok, status->battery_percent, (unsigned long)status->dropped_batches,
             (unsigned long)status->sensor_faults, (unsigned long)status->storage_faults,
             status->state, (unsigned long)status->fault_mask);
}
