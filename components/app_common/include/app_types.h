#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_SAMPLE_BATCH_SIZE 32U
#define APP_HORSE_NAME_MAX 32U

typedef struct {
    uint32_t sequence;
    uint64_t timestamp_us;
    uint32_t ppg_1;
    uint32_t ppg_2;
} app_ppg_sample_t;

typedef struct {
    size_t count;
    app_ppg_sample_t samples[APP_SAMPLE_BATCH_SIZE];
} app_sample_batch_t;

typedef enum {
    APP_COMMAND_START_RECORDING,
    APP_COMMAND_STOP_RECORDING,
    APP_COMMAND_SET_TIME,
    APP_COMMAND_SET_HORSE,
    APP_COMMAND_ENTER_UPLOAD_MODE,
    APP_COMMAND_DELETE_DATA,
    APP_COMMAND_RESTART,
} app_command_id_t;

typedef struct {
    app_command_id_t id;
    char payload[96];
} app_command_t;

typedef enum {
    APP_STATE_BOOT,
    APP_STATE_IDLE,
    APP_STATE_RECORDING,
    APP_STATE_FAULT,
} app_state_t;

typedef struct {
    bool ppg_1_ok;
    bool ppg_2_ok;
    bool accelerometer_ok;
    bool sd_ok;
    bool recording;
    bool storage_fault;
    uint8_t battery_percent;
    uint32_t dropped_batches;
    uint32_t sensor_faults;
    uint32_t storage_faults;
    uint32_t fault_mask;
    app_state_t state;
} app_status_t;

typedef enum {
    APP_FAULT_NONE = 0,
    APP_FAULT_SENSOR_START = 1U << 0,
    APP_FAULT_SENSOR_RUNTIME = 1U << 1,
    APP_FAULT_STORAGE_START = 1U << 2,
    APP_FAULT_STORAGE_RUNTIME = 1U << 3,
    APP_FAULT_QUEUE_OVERFLOW = 1U << 4,
} app_fault_t;
