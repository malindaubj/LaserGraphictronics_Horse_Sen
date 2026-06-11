# Firmware Engineering Reference

## Architecture Summary

"The firmware uses a layered ESP-IDF architecture. The BSP isolates board
details, services encapsulate sensor, storage, and communication mechanisms,
and `app_core` owns product behavior. A high-priority sensor task produces
paired timestamped batches into a bounded queue. A lower-priority storage task
owns the SD file and consumes those batches. Commands enter through a separate
queue, and event groups distribute recording and fault state."

## Why Not One Large Task?

A large task couples unrelated timing. An SD write or network timeout can then
delay FIFO draining and lose samples. Separate tasks let priorities represent
deadlines and queues absorb short latency spikes.

## Queue vs Mutex vs Event Group

- Queue: transfers ownership of commands or sample batches.
- Mutex: protects a shared status snapshot during a short critical section.
- Event group: broadcasts persistent state such as recording or fatal fault.
- Task notification: preferred ISR-to-task wakeup for future PPG interrupts.

## Priority Inversion

Priority inversion occurs when a high-priority task waits for a lock held by a
low-priority task. FreeRTOS mutexes provide priority inheritance, but the
stronger design here is avoiding shared peripheral locks through single-owner
tasks.

## ISR Rules

Keep ISRs deterministic and short. Do not allocate memory, log heavily, access
SD, or perform I2C transactions. Clear the source, notify a task using an
`FromISR` API, request a context switch if required, and return.

In this project, an ESP32-S3 GPTimer alarm fires every 20 seconds while
recording. The ISR calls `vTaskNotifyGiveFromISR()` and the sensor task performs
the complete acquisition window. This demonstrates deferred interrupt
processing.

## Backpressure

The sample queue is bounded. If storage falls behind, the sensor task counts
dropped batches. This makes overload measurable. In production, status can
stop the session when the loss threshold is exceeded.

## Timing and Data Integrity

Use monotonic microsecond timestamps, not assumed loop timing. Pair only fresh
samples from both PPG sensors. Track FIFO overflow and sequence numbers. Never
print every sample over a slow serial port in the acquisition path.

## Watchdog and Fault Recovery

Tasks must block or delay normally so the idle tasks can run. Long operations
need bounded timeouts. Fault policy should distinguish transient, degraded, and
fatal faults; retry transient failures and safely stop recording on integrity
failures.

## Security Answer

Never hardcode destructive-command passwords or disable TLS verification.
Authenticate sessions, authorize commands, protect against replay, store
credentials in encrypted NVS, verify server certificates, and use signed OTA
images with rollback.

## Honest Project Limitations

- SPI pins must be confirmed from the schematic.
- MAX30101 interrupt pins were not present in V2, so the baseline polls FIFO.
- NimBLE and WiFi upload adapters are the next communication-layer ports.
- The local ESP-IDF Python environment is incomplete, so hardware build and
  flash validation remain required.
