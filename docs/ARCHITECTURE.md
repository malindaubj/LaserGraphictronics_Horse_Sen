# Architecture and Design Walkthrough

## 1. Why the Firmware Was Rebuilt

The V2 sketch mixed drivers, BLE parsing, sensor acquisition, SD access,
uploading, LEDs, and product state in one compilation unit. Multiple tasks and
BLE callbacks could change shared pointers and use SD/SPI concurrently.

V3 applies four rules:

1. A peripheral has one owner.
2. Tasks communicate using typed queues and event groups.
3. Timing-critical acquisition never performs storage or network work.
4. Every overload or failure is observable.

## 2. Layers

### Board Support Package

`board_config` contains physical board knowledge such as pins and electrical
assumptions. `bsp` exposes sensor power, I2C transport, ADC conversion, and
LED primitives. Higher layers do not call ESP-IDF GPIO or I2C APIs. That makes
a board revision primarily a board configuration and BSP review.

### Driver and Service Layer

Dedicated `max30101`, `bmi270`, and `pca9546a` components own register-level
device protocols. `sensor_service` composes those drivers into paired fresh
samples, timestamps them with the ESP-IDF microsecond timer, and returns batches.

`storage_service` mounts FATFS and owns the active CSV file. It exposes session
operations rather than raw `FILE *` handles, preventing application code from
breaking file ownership.

`comm_service` translates an external transport into typed commands and
publishes status. The baseline uses UART so the project is immediately
testable. NimBLE and WiFi belong here as adapters.

### Application Layer

`app_core` owns product state and creates all RTOS objects and tasks. It decides
when recording starts and stops, but it does not know sensor registers or CSV
details.

## 3. RTOS Concepts Used

### Tasks and Priorities

The sensor task has the highest application priority and is pinned to core 1.
Storage and communication run on core 0. Priority reflects deadline, not
importance: acquisition must drain FIFOs before they overflow.

A hardware GPTimer generates a periodic alarm every 20 seconds while recording
is enabled. `START` releases the first collection window immediately. Later
alarms release consecutive 20-second collection windows.

### Bounded Queues

The sample queue decouples sensor timing from SD latency. It is deliberately
bounded. If SD cannot keep up, the sensor task drops a complete batch and
increments `dropped_batches`. A bounded failure is safer than unbounded memory
growth or hidden timing distortion.

The command queue serializes commands from communications into the supervisor,
eliminating command callbacks that mutate application state concurrently.

### Event Groups

Event bits represent level-triggered system state such as recording and fatal
fault. They are suitable when multiple tasks need to observe the same state.
Queues are used for data that must be consumed exactly once.

### Mutex

The status mutex protects the shared status snapshot. Drivers and files do not
need broad mutexes because they use strict single-task ownership.

## 4. Interrupt Strategy

An ISR should do the minimum possible work:

1. Capture/clear the hardware interrupt condition.
2. Use `vTaskNotifyGiveFromISR()` to wake the sensor task.
3. Call `portYIELD_FROM_ISR()` when a higher-priority task was woken.

I2C transactions, logging, memory allocation, and SD writes must never happen
inside an ISR.

V3 currently polls the MAX30101 FIFO because V2 does not identify interrupt
GPIOs. Once the schematic supplies those pins, the polling delay should be
replaced by ISR-to-task notifications without changing the storage or
application layers.

The periodic collection scheduler already follows this interrupt pattern. Its
GPTimer ISR sends a direct task notification and returns; the sensor task owns
the complete 20-second acquisition window.

## 5. Data Integrity

V2 created a row whenever either PPG FIFO had data and reused the other
sensor's previous value. V3 emits a row only after receiving a fresh sample
from both sensors. Timestamps come from `esp_timer_get_time()` instead of an
assumed sample index.

For a production research instrument, the next enhancement is to add sequence
counters, FIFO overflow counters, monotonic session IDs, and a file trailer
containing row count and CRC.

## 6. Fault Handling

Startup checks sensor and storage health. A required-device failure sets the
fatal-fault event and blocks recording. Runtime storage failures are latched in
status. The supervisor periodically publishes a snapshot and drives status
LEDs.

A production build should define fault severity and recovery policy in a table:
retry transient faults, degrade optional devices, and safely stop recording on
data-integrity faults.

## 7. Security

V2 embedded a destructive-command password and accepted insecure TLS. Those
patterns should not ship. Production commands need authenticated sessions,
authorization, replay protection, encrypted NVS credentials, verified TLS
certificates, and signed OTA images.

## 8. Testing Strategy

### Host Tests

- Command/state transition tests.
- Paired-sample and FIFO overflow tests with mocked BSP calls.
- CSV formatting and session lifecycle tests.
- Queue-overload and fault-latching tests.

### Hardware-in-Loop Tests

- Disconnect each sensor during recording.
- Remove/fill/corrupt the SD card.
- Measure sample timing and dropped batches under worst-case SD latency.
- Run BLE/WiFi traffic while recording.
- Power-cycle during every storage operation.
- Run 24-hour soak and memory-leak tests.

## 9. Design Summary

"I separated policy from mechanism. The BSP knows the board, services know
devices, and the application knows product behavior. I gave each peripheral one
owner and used queues and events across task boundaries. The acquisition task
has the highest priority because it has a real deadline, while SD and
communications are asynchronous. Backpressure and faults are measured instead
of hidden. The remaining hardware-dependent step is wiring FIFO interrupts into
task notifications."
