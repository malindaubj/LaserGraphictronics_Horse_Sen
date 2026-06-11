#include "app_core.h"

#include <string.h>

#include <FreeRTOS.h>
#include <event_groups.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "app_config.h"
#include "app_types.h"
#include "bsp.h"
#include "comm_service.h"
#include "driver/gptimer.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sensor_service.h"
#include "storage_service.h"

static const char *TAG = "app_core";

static EventGroupHandle_t s_events;
static QueueHandle_t s_sample_queue;
static QueueHandle_t s_command_queue;
static SemaphoreHandle_t s_status_lock;
static gptimer_handle_t s_collection_timer;
static TaskHandle_t s_sensor_task_handle;
static app_status_t s_status;
static app_state_t s_state = APP_STATE_BOOT;
static uint32_t s_fault_mask;
static char s_horse_name[APP_HORSE_NAME_MAX] = "Unknown";
static bool s_collection_timer_running;
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;

static void collection_timer_stop(void);

static void latch_fault(app_fault_t fault) {
    taskENTER_CRITICAL(&s_state_lock);
    s_fault_mask |= (uint32_t)fault;
    s_state = APP_STATE_FAULT;
    taskEXIT_CRITICAL(&s_state_lock);
    xEventGroupClearBits(s_events, APP_EVENT_RECORDING);
    collection_timer_stop();
    xEventGroupSetBits(s_events, APP_EVENT_FATAL_FAULT);
}

/*
 * Hardware timer ISR callback. Keep this deterministic: it only releases the
 * next collection cycle. Sensor I2C and storage work remain in task context.
 */
static bool IRAM_ATTR collection_timer_alarm_cb(gptimer_handle_t timer,
                                                const gptimer_alarm_event_data_t *event,
                                                void *context) {
    (void)timer;
    (void)event;
    (void)context;
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (s_sensor_task_handle != NULL) {
        vTaskNotifyGiveFromISR(s_sensor_task_handle, &higher_priority_task_woken);
    }
    return higher_priority_task_woken == pdTRUE;
}

static esp_err_t collection_timer_init(void) {
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000U,
    };
    ESP_RETURN_ON_ERROR(gptimer_new_timer(&timer_config, &s_collection_timer), TAG,
                        "collection timer creation failed");

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = collection_timer_alarm_cb,
    };
    ESP_RETURN_ON_ERROR(gptimer_register_event_callbacks(s_collection_timer, &callbacks, NULL), TAG,
                        "collection timer callback failed");

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = APP_COLLECTION_PERIOD_MS * 1000ULL,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_RETURN_ON_ERROR(gptimer_set_alarm_action(s_collection_timer, &alarm_config), TAG,
                        "collection timer alarm failed");
    return gptimer_enable(s_collection_timer);
}

static esp_err_t collection_timer_start(void) {
    if (s_collection_timer_running) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(gptimer_set_raw_count(s_collection_timer, 0), TAG,
                        "collection timer reset failed");
    ESP_RETURN_ON_ERROR(gptimer_start(s_collection_timer), TAG, "collection timer start failed");
    s_collection_timer_running = true;
    return ESP_OK;
}

static void collection_timer_stop(void) {
    if (s_collection_timer_running) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(gptimer_stop(s_collection_timer));
        s_collection_timer_running = false;
    }
    if (s_sensor_task_handle != NULL) {
        xTaskNotifyGive(s_sensor_task_handle);
    }
}

static void status_update_health(void) {
    sensor_health_t health = sensor_service_health();
    xSemaphoreTake(s_status_lock, portMAX_DELAY);
    s_status.ppg_1_ok = health.ppg_1_ok;
    s_status.ppg_2_ok = health.ppg_2_ok;
    s_status.accelerometer_ok = health.accelerometer_ok;
    s_status.sd_ok = storage_service_is_ready();
    s_status.battery_percent = bsp_battery_percent();
    xSemaphoreGive(s_status_lock);
}

static void sensor_task(void *context) {
    (void)context;
    bool running = false;
    int64_t window_deadline_us = 0;
    for (;;) {
        bool recording = (xEventGroupGetBits(s_events) & APP_EVENT_RECORDING) != 0U;
        if (!running) {
            /*
             * The first notification comes from START; subsequent notifications
             * come directly from the 20-second GPTimer ISR.
             */
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            recording = (xEventGroupGetBits(s_events) & APP_EVENT_RECORDING) != 0U;
        }

        if (recording && !running) {
            if (sensor_service_start() == ESP_OK) {
                running = true;
                window_deadline_us =
                    esp_timer_get_time() + ((int64_t)APP_COLLECTION_WINDOW_MS * 1000LL);
                ESP_LOGI(TAG, "20-second sensor acquisition window started");
            } else {
                xSemaphoreTake(s_status_lock, portMAX_DELAY);
                s_status.sensor_faults++;
                xSemaphoreGive(s_status_lock);
                latch_fault(APP_FAULT_SENSOR_START);
            }
        }

        if (running && (!recording || esp_timer_get_time() >= window_deadline_us)) {
            sensor_service_stop();
            running = false;
            ESP_LOGI(TAG, "Sensor acquisition window stopped");
        }
        if (!running) {
            continue;
        }

        app_sample_batch_t batch;
        esp_err_t read_result = sensor_service_read_batch(&batch);
        if (read_result != ESP_OK && read_result != ESP_ERR_TIMEOUT) {
            xSemaphoreTake(s_status_lock, portMAX_DELAY);
            s_status.sensor_faults++;
            xSemaphoreGive(s_status_lock);
            latch_fault(APP_FAULT_SENSOR_RUNTIME);
            continue;
        }
        if (read_result == ESP_OK &&
            xQueueSend(s_sample_queue, &batch, APP_STORAGE_QUEUE_TIMEOUT) != pdPASS) {
            xSemaphoreTake(s_status_lock, portMAX_DELAY);
            s_status.dropped_batches++;
            xSemaphoreGive(s_status_lock);
            ESP_LOGW(TAG, "Storage backpressure: sample batch dropped");
            latch_fault(APP_FAULT_QUEUE_OVERFLOW);
        }
    }
}

static void storage_task(void *context) {
    (void)context;
    bool session_open = false;
    for (;;) {
        EventBits_t bits = xEventGroupGetBits(s_events);
        if ((bits & APP_EVENT_RECORDING) != 0U && !session_open) {
            session_open = storage_service_begin_session(s_horse_name) == ESP_OK;
            if (!session_open) {
                latch_fault(APP_FAULT_STORAGE_START);
            }
        }

        app_sample_batch_t batch;
        if (xQueueReceive(s_sample_queue, &batch, pdMS_TO_TICKS(100)) == pdPASS && session_open &&
            storage_service_write_batch(&batch) != ESP_OK) {
            xSemaphoreTake(s_status_lock, portMAX_DELAY);
            s_status.storage_fault = true;
            s_status.storage_faults++;
            xSemaphoreGive(s_status_lock);
            latch_fault(APP_FAULT_STORAGE_RUNTIME);
        }

        bits = xEventGroupGetBits(s_events);
        if ((bits & APP_EVENT_RECORDING) == 0U && session_open) {
            storage_service_end_session();
            session_open = false;
            xQueueReset(s_sample_queue);
        }
    }
}

static void handle_command(const app_command_t *command) {
    switch (command->id) {
    case APP_COMMAND_START_RECORDING:
        if ((xEventGroupGetBits(s_events) & APP_EVENT_FATAL_FAULT) == 0U) {
            xEventGroupSetBits(s_events, APP_EVENT_RECORDING);
            /* Start the first cycle immediately; GPTimer releases later cycles. */
            if (s_sensor_task_handle != NULL) {
                xTaskNotifyGive(s_sensor_task_handle);
            }
            if (collection_timer_start() == ESP_OK) {
                s_state = APP_STATE_RECORDING;
            } else {
                latch_fault(APP_FAULT_SENSOR_START);
            }
        }
        break;
    case APP_COMMAND_STOP_RECORDING:
        xEventGroupClearBits(s_events, APP_EVENT_RECORDING);
        collection_timer_stop();
        if (s_fault_mask == 0U)
            s_state = APP_STATE_IDLE;
        break;
    case APP_COMMAND_SET_HORSE:
        if ((xEventGroupGetBits(s_events) & APP_EVENT_RECORDING) == 0U) {
            strlcpy(s_horse_name, command->payload, sizeof(s_horse_name));
        }
        break;
    case APP_COMMAND_RESTART:
        esp_restart();
        break;
    default:
        ESP_LOGW(TAG, "Command %d is not implemented by this build", command->id);
        break;
    }
}

static void supervisor_task(void *context) {
    (void)context;
    TickType_t last_health = 0;
    for (;;) {
        app_command_t command;
        if (xQueueReceive(s_command_queue, &command, pdMS_TO_TICKS(100)) == pdPASS) {
            handle_command(&command);
        }
        TickType_t now = xTaskGetTickCount();
        if ((now - last_health) >= pdMS_TO_TICKS(APP_HEALTH_PERIOD_MS)) {
            last_health = now;
            status_update_health();
            xSemaphoreTake(s_status_lock, portMAX_DELAY);
            s_status.recording = (xEventGroupGetBits(s_events) & APP_EVENT_RECORDING) != 0U;
            taskENTER_CRITICAL(&s_state_lock);
            s_status.state = s_state;
            s_status.fault_mask = s_fault_mask;
            taskEXIT_CRITICAL(&s_state_lock);
            app_status_t snapshot = s_status;
            xSemaphoreGive(s_status_lock);
            comm_service_publish_status(&snapshot);
            ESP_LOGD(TAG, "state=%d faults=0x%08lx sensor_stack=%u storage_stack=%u", s_state,
                     (unsigned long)s_fault_mask,
                     (unsigned)uxTaskGetStackHighWaterMark(s_sensor_task_handle),
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
            bsp_led_set(BSP_LED_RED, (xEventGroupGetBits(s_events) & APP_EVENT_FATAL_FAULT) != 0U);
            bsp_led_set(BSP_LED_GREEN, snapshot.recording);
        }
    }
}

void app_core_start(void) {
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES || nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);
    ESP_ERROR_CHECK(bsp_init());

    s_events = xEventGroupCreate();
    s_sample_queue = xQueueCreate(APP_SAMPLE_QUEUE_LENGTH, sizeof(app_sample_batch_t));
    s_command_queue = xQueueCreate(APP_COMMAND_QUEUE_LENGTH, sizeof(app_command_t));
    s_status_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(
        (s_events && s_sample_queue && s_command_queue && s_status_lock) ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(collection_timer_init());

    esp_err_t sensor_result = sensor_service_init();
    esp_err_t storage_result = storage_service_init();
    status_update_health();
    if (sensor_result != ESP_OK || storage_result != ESP_OK) {
        latch_fault(sensor_result != ESP_OK ? APP_FAULT_SENSOR_START : APP_FAULT_STORAGE_START);
        ESP_LOGE(TAG, "Startup health check failed: sensors=%s storage=%s",
                 esp_err_to_name(sensor_result), esp_err_to_name(storage_result));
    }
    if (s_fault_mask == 0U)
        s_state = APP_STATE_IDLE;
    ESP_ERROR_CHECK(comm_service_init(s_command_queue));

    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(sensor_task, "sensor", APP_SENSOR_TASK_STACK, NULL,
                                            APP_SENSOR_TASK_PRIORITY, &s_sensor_task_handle,
                                            APP_SENSOR_TASK_CORE) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(storage_task, "storage", APP_STORAGE_TASK_STACK, NULL,
                                            APP_STORAGE_TASK_PRIORITY, NULL,
                                            APP_SERVICE_TASK_CORE) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(
        xTaskCreatePinnedToCore(supervisor_task, "supervisor", APP_SUPERVISOR_TASK_STACK, NULL,
                                APP_SUPERVISOR_TASK_PRIORITY, NULL, APP_SERVICE_TASK_CORE) == pdPASS
            ? ESP_OK
            : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "DigiEquine V3 started");
}
