# Changelog — SolarController_EP_LS1024B

Device-facing decisions for the EPEVER LandStar LS1024B driver. Entries record
*why* a rule exists and where it came from, not just what the code does: this
driver writes battery charge settings, so an undocumented assumption here can
damage a battery.

Newest first. The library has not been released beyond the bench, so everything
so far is one unreleased version.

## [0.1.0] — 2026-09-23 — bench driver, initial

Unreleased. Only the `pcv_solar_test` bench target links it.

### Added

- **Measurements** over FC04: PV voltage/current, battery voltage and charging
  current, load voltage/current, battery and device temperature (signed, 0.01 C),
  battery SOC, and the raw battery/charging status words.
- **Settings read** over FC03, covering `0x9000..0x900E`, `0x903D`, `0x9067`, and
  `0x90BF`.
- **Charge setpoint write**: the twelve setpoints at `0x9003..0x900E` are written
  as a single FC10 "write multiple registers" request, then the whole block is
  read back.
- **Address change** using the proprietary EPEVER service command `0x45`, with
  success decided by probing the new and previous addresses afterwards.
- **Bench console** (`lib/SolarControllerTestConsole/`) exposing all of the above
  as `solar …` serial commands.

### Changed

- **`lib/Rs485ModBus` now frames FC10 responses.** `RS485Bus::selectResponseFrame()`
  modelled 0x03, 0x04, and 0x06 only, because the DELIXI CDI-E100 (its original
  consumer) needs nothing else. An FC10 reply would have fallen into the
  last-resort diagnostic path and been misread. An FC10 response is eight bytes —
  slave, function, starting address, quantity, CRC — per the MODBUS Application
  Protocol Specification V1.1b3 definition for function 0x10. The change is
  additive: no existing case was altered.
- **Writes were reworked from FC06 to FC10.** The first implementation wrote each
  setpoint with its own FC06 request (twelve transactions). The LS1024B
  configuration sketch states that a complete `0x9003..0x900E` block write is
  *required*, because the twelve values constrain each other. Per-register writes
  produce intermediate states that the controller may reject. The FC06 write path
  was deleted rather than kept as a way to do the wrong thing.

### Design decisions and their sources

- **Where each register address comes from.** Real-time and setting addresses come
  from the LS1024B configuration sketches; battery voltage (0x3104), battery
  temperature (0x3110), load current (0x310D), and battery status (0x3200) are
  proven on the installed controller. The wider map is flagged
  `REALTIME_BLOCK_HARDWARE_CONFIRMED = false` and
  `SETTINGS_MAP_HARDWARE_CONFIRMED = false` rather than presented as confirmed.
  Full per-row provenance is in `docs/EPEVER_LS1024B.md`.
- **Three gates before any write.** (1) The block must satisfy
  `voltageBlockOrdered()`, the ordering rule from the configuration sketch, kept
  verbatim; it is also asserted at compile time against the configured profile.
  (2) The controller must already report battery type `0` (user-defined).
  (3) It must report rated voltage level `1` (12 V). Checks 2 and 3 exist because
  these setpoints mean "user-defined 12 V battery" values; writing them onto a
  factory battery type or a 24 V system would be wrong. Failing a gate sends
  nothing.
- **A matching block is not rewritten.** The setpoints live in controller EEPROM,
  so an identical block is left alone to avoid needless write cycles.
- **`APPLIED` requires read-back.** The FC10 echo only proves the controller
  accepted the frame, so all twelve registers are read back after a 250 ms settle
  delay. Anything less than all twelve reports `VERIFICATION_MISMATCH` and the
  stored settings must be treated as uncertain.
- **Reads stay within twelve registers.** Every request in the reference
  implementation is twelve registers or fewer, so the driver never asks for more.
  This is an envelope taken from the working reference, not a documented device
  limit — see open items.
- **Blocks that do not answer are reported as `NOT_ANSWERED`,** never as measured
  zeroes, so a silent controller cannot look like a flat battery.
- **The address change is not Modbus.** Function `0x45` is a proprietary,
  broadcast EPEVER service command captured from the official PC tool; its
  response format is undocumented, so responses are recorded byte-for-byte and
  never parsed. Success is decided the way the PC tool decides it: the new
  address answers a battery-voltage read and the previous address stops
  answering. Both captured frames are asserted byte-for-byte in
  `test/test_protocol/test_main.cpp`.

### Not written by this firmware

Battery type (`0x9000`), capacity (`0x9001`), temperature compensation
(`0x9002`), rated voltage level (`0x9067`), and maximum charging current
(`0x90BF`) are **read only** here. They must be set on the controller itself.
The configuration sketch also writes only the twelve setpoints, and treats
battery type and rated voltage as preconditions.

### Removed

- FC06 single-register writes and the `applyProfile()` API they served, replaced
  by `applyVoltageBlock()`. A profile that writes a temperature-compensation
  encoding (a guessed sign and scale) went with them; nothing in either sketch
  writes that register either.

### Open items

- The settings map is unconfirmed against an LS1024B document. The bench read-back
  and a comparison with the controller display are what settle it.
- Whether the LS1024B accepts FC10 at all, and how many registers it allows, is a
  device question. The configuration sketch uses FC10 for twelve registers, which
  is evidence but not proof.
- The twelve-register read envelope may be a property of the reference
  implementation's buffers rather than the device.
- Equalization and boost duration registers are unknown for this family and are
  not written.
- PV, battery, and load power registers are not decoded: their 32-bit word order
  is undocumented here.
- The `0x3200`/`0x3201` status bits are reported raw and not interpreted.

### Verified how

- `test/test_protocol` locks the captured service frames (`F8 45 00 01 01 F8 89 BE`
  and `F8 45 00 01 01 60 88 14`), the documented EPEVER read frames, the FC10
  setpoint frame, every block-read frame, the coefficient-100 encoding, and the
  accept/reject cases of the ordering rule. These are compile-time assertions, so
  they run on every build.
- The Unity runtime assertions in that file have not been executed: they need the
  board attached.
- Nothing in this driver has been run against a solar controller yet.
