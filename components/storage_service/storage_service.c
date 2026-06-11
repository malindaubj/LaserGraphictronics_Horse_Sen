#include "storage_service.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "board_config.h"
#include "data_integrity.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

static const char *TAG = "storage";
static const char *MOUNT_POINT = "/sd";
static FILE *s_session_file;
static bool s_ready;
static uint32_t s_rows_written;
static uint32_t s_rolling_checksum;
static uint32_t s_batches_since_sync;

esp_err_t storage_service_init(void) {
    spi_bus_config_t bus_config = {
        .mosi_io_num = BOARD_SD_MOSI_GPIO,
        .miso_io_num = BOARD_SD_MISO_GPIO,
        .sclk_io_num = BOARD_SD_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t bus_result = spi_bus_initialize(BOARD_SD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (bus_result != ESP_OK && bus_result != ESP_ERR_INVALID_STATE) {
        return bus_result;
    }
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = BOARD_SD_CS_GPIO;
    slot.host_id = host.slot;
    sdmmc_card_t *card = NULL;
    esp_err_t result = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot, &mount_config, &card);
    s_ready = result == ESP_OK;
    if (s_ready) {
        mkdir("/sd/data", 0775);
    }
    return result;
}

esp_err_t storage_service_begin_session(const char *horse_name) {
    if (!s_ready || s_session_file != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    struct timeval now;
    gettimeofday(&now, NULL);
    char path[160];
    snprintf(path, sizeof(path), "/sd/data/session_%lld.csv", (long long)now.tv_sec);
    s_session_file = fopen(path, "w");
    if (s_session_file == NULL) {
        return ESP_FAIL;
    }
    s_rows_written = 0;
    s_batches_since_sync = 0;
    s_rolling_checksum = DATA_INTEGRITY_FNV1A_INITIAL;
    fprintf(s_session_file, "# format_version=1,device=DigiEquineV3,horse=%s\n",
            horse_name ? horse_name : "Unknown");
    fputs("sequence,timestamp_us,ppg_1,ppg_2\n", s_session_file);
    return ESP_OK;
}

esp_err_t storage_service_write_batch(const app_sample_batch_t *batch) {
    if (batch == NULL || s_session_file == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    for (size_t i = 0; i < batch->count; ++i) {
        const app_ppg_sample_t *sample = &batch->samples[i];
        if (fprintf(s_session_file, "%lu,%llu,%lu,%lu\n", (unsigned long)sample->sequence,
                    (unsigned long long)sample->timestamp_us, (unsigned long)sample->ppg_1,
                    (unsigned long)sample->ppg_2) < 0) {
            return ESP_FAIL;
        }
        s_rolling_checksum = data_integrity_fnv1a_update(s_rolling_checksum, &sample->sequence,
                                                         sizeof(sample->sequence));
        s_rolling_checksum = data_integrity_fnv1a_update(s_rolling_checksum, &sample->timestamp_us,
                                                         sizeof(sample->timestamp_us));
        s_rolling_checksum =
            data_integrity_fnv1a_update(s_rolling_checksum, &sample->ppg_1, sizeof(sample->ppg_1));
        s_rolling_checksum =
            data_integrity_fnv1a_update(s_rolling_checksum, &sample->ppg_2, sizeof(sample->ppg_2));
        s_rows_written++;
    }
    if (++s_batches_since_sync >= 16U) {
        s_batches_since_sync = 0;
        if (fflush(s_session_file) != 0 || fsync(fileno(s_session_file)) != 0) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t storage_service_end_session(void) {
    if (s_session_file == NULL) {
        return ESP_OK;
    }
    fprintf(s_session_file, "# rows=%lu,checksum_fnv1a=%08lx\n", (unsigned long)s_rows_written,
            (unsigned long)s_rolling_checksum);
    int flush_result = fflush(s_session_file);
    int sync_result = fsync(fileno(s_session_file));
    int close_result = fclose(s_session_file);
    s_session_file = NULL;
    return (flush_result == 0 && sync_result == 0 && close_result == 0) ? ESP_OK : ESP_FAIL;
}

bool storage_service_is_ready(void) {
    return s_ready;
}
