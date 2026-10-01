# PumpControl changelog

## 2026-10-01

### Added

- Bluetooth Classic Serial (SPP) console for USB-free diagnostics while the pump
  runs. It carries the same log stream and command set as USB Serial, reuses the
  shared password, and prompts on every client connection so a deployed node
  needs no reset to unlock. Enabled with `PUMP_BT_CONSOLE=1` in `platformio.ini`;
  set to 0 to compile the wireless channel out.

### Notes

- The wireless console is not a substitute for the hardwired stop and safe
  isolation arrangement. Verbose Modbus tracing (`debug full`) over Bluetooth
  can stall the control loop while the pump runs.
- Build cost with the console enabled: flash 89.0% (1,165,998 B of 1,310,720 B),
  RAM 14.2%. Disabled (`PUMP_BT_CONSOLE=0`): flash 29.8%, RAM 8.2%.


## 2026-09-24

### Added

- Readable RadioLib error messages with OTAA attempt counts and randomized,
  bounded retry delays.
- Protocol-v2 status fields for previous join error, join-attempt count, and
  previous retry delay; protocol-v1 decoding remains supported.
- Non-actuating `01 05` status-refresh command with a 10-second dashboard rate
  limit.
- Two-stage remote Start/Stop reporting: `in_progress` after VFD acceptance,
  then a final result after the measured VFD condition is reached.
- Final Start confirmation within 0.25 Hz of the requested frequency and final
  Stop confirmation at `Stopped` and at most 0.25 Hz, with a 120-second failure
  timeout.

### Changed

- Periodic status uses a 15-second interval while running and a 60-second
  interval while stopped; commands, refresh requests, important state changes,
  and final command results still trigger prompt reports.
- Dashboard marks telemetry offline after 45 seconds when last reported running
  or 75 seconds when last reported stopped, hides stale radio/VFD health values,
  and blocks non-Stop commands until a fresh status arrives.
- Dashboard shows compact colored LoRaWAN state and an icon-only refresh control
  below the Pump subtitle.
- Last-uplink and command-queue timestamps use readable
  `DD/MM/YYYY HH:mm:ss` formatting.

### Validation

- `platformio run -e pump_control` succeeds.
- Dashboard JSON, Function-node syntax, codec compatibility, refresh routing,
  timestamp conversion, adaptive offline thresholds, and command-progress
  handling were checked.
