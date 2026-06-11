"""Repository-level architecture checks that run without target hardware."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"ERROR: {message}", file=sys.stderr)
        raise SystemExit(1)


def main() -> None:
    board_config = (ROOT / "components/board_config/include/board_config.h").read_text()
    sensor_service = (ROOT / "components/sensor_service/sensor_service.c").read_text()
    app_core = (ROOT / "components/app_core/app_core.c").read_text()

    require("BOARD_SD_CS_GPIO" in board_config, "SD pins must live in board_config.h")
    require("REG_FIFO_DATA" not in sensor_service, "services must not contain device registers")
    require("max30101_read_sample" in sensor_service, "sensor service must use the device driver")
    require("vTaskNotifyGiveFromISR" in app_core, "timer ISR must defer work to a task")

    callback_start = app_core.index("collection_timer_alarm_cb")
    callback_end = app_core.index("static esp_err_t collection_timer_init")
    callback = app_core[callback_start:callback_end]
    for forbidden in ("bsp_i2c_", "storage_service_", "ESP_LOG", "malloc", "free("):
        require(forbidden not in callback, f"ISR contains forbidden operation: {forbidden}")

    print("Architecture checks passed")


if __name__ == "__main__":
    main()
