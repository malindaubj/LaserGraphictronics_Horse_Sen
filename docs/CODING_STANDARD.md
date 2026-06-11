# Firmware Coding Standard

- Use fixed-width integer types for protocol, register, and stored data.
- Check every fallible driver and storage operation.
- Never perform I2C, SPI, file I/O, logging, or allocation inside an ISR.
- Give each peripheral and mutable resource one task owner.
- Use bounded queues and define overload behavior.
- Keep application policy independent from board pins and device registers.
- Avoid runtime allocation after startup.
- Use monotonic timestamps for durations and ordering.
- Latch and report data-integrity faults; stop recording when integrity cannot
  be guaranteed.
- Document provisional hardware assumptions in `board_config.h`.
- Require review and evidence before changing timing, task priority, or stored
  data format.
