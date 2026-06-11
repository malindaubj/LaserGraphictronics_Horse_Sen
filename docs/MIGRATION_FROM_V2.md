# Migration Notes

## Preserved

- ESP32-S3 target.
- PCA9546A mux channels: PPG 1 on 0, PPG 2 on 1, BMI270 on 2.
- Dual MAX30101 acquisition.
- SD-backed CSV recording.
- Battery measurement and status LEDs.
- External start/stop and horse-name commands.

## Intentionally Changed

- Arduino framework replaced by ESP-IDF.
- One large global sketch replaced by components.
- Dynamic monolithic PPG buffer replaced by fixed-size queued batches.
- Synthetic timestamps replaced by monotonic microsecond timestamps.
- Stale cross-sensor sample reuse removed.
- Direct callback state mutation replaced by queued commands.
- Concurrent SD access removed through single-task ownership.
- Per-sample serial printing removed.

## To Port From the Mobile App

Implement a NimBLE adapter inside `comm_service` with the existing UUIDs. BLE
write callbacks should parse and validate the message, create `app_command_t`,
and enqueue it. They must not mount SD, start WiFi, allocate large buffers, or
change acquisition state directly.
