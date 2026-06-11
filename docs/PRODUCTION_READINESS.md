# Production Readiness

## Release Gates

A firmware build is releasable only when all gates below have evidence attached
to the release record.

### Design

- PCB schematic and `board_config.h` reviewed together.
- Device datasheet register settings reviewed.
- Task priorities, stack sizes, and worst-case execution times measured.
- Failure-mode and recovery behavior approved.

### Build Quality

- CI build succeeds from a clean checkout.
- Compiler warnings are zero.
- Formatting and static analysis pass.
- Firmware version, build ID, and source revision are recorded.

### Verification

- Unit and integration tests pass.
- Hardware-in-loop test suite passes on the released PCB revision.
- PPG timing, FIFO overflow, and dropped-batch counters meet requirements.
- SD removal, full-card, corrupt-card, and power-loss tests pass.
- Twenty-four-hour soak test shows no leak, watchdog reset, or data corruption.
- Stack high-water marks retain the approved safety margin.

### Security

- Secure boot and flash encryption policy approved.
- Signed OTA with rollback tested.
- Credentials stored in encrypted NVS.
- TLS certificate verification enabled.
- Destructive commands require authenticated authorization.

## Known Open Hardware Items

- Confirm SD SPI pins in `board_config.h`.
- Confirm LED active level.
- Identify MAX30101 interrupt GPIOs.
- Validate battery divider and ADC calibration.
- Complete BMI270 initialization using the vendor-supported driver.

## Definition of Done

The current repository is a production-oriented firmware baseline. It becomes a
production release only after the hardware-dependent gates above have measured
evidence and formal approval.
