# Pressure-control end node

## Implemented Prototype Rev A

The pressure-control valve firmware is separate from the existing pump/VFD end
node. Three PlatformIO environments preserve the assembled dual-I2C prototype
and use one common application API for Serial, LoRaWAN and future transports:
`pcv_serial_only`, `pcv_hybrid_class_c`, and `pcv_low_power_class_a`.

Implemented modules:

- `PressureNodeCore`: typed readings, PCV state/verification, polarity checks,
  combined status, and `Active`/`Idle`/`SleepReady` power policy;
- `BatteryMonitor`: averaged GPIO35 voltage, 100 nF input-settling allowance,
  measured divider calibration, raw ADC-pin voltage diagnostics, and explicit
  unknown state of charge;
- `PressureSensorXDB401`: independent `TwoWire` injection, address discovery,
  repeated-start register access, timeout/error status, and explicitly
  unvalidated engineering scale;
- `PressureControlValve`: safe L298N power sequencing, opposite-polarity
  latching pulses, and `Unknown` reset state;
- `FlowMeter`: transport-independent flow reading interface;
- `Tuf2000mFlowMeter`: manual-derived Modbus RTU function-03 reader for
  on-demand flow rate, velocity, device error bits, and net/positive/negative
  accumulated volume over the reusable `Rs485ModBus` transport;
- `PressureControlNode`: shared application/status service;
- `PressureNodeCommandProcessor`: transport-independent command parser plus a
  Serial line source;
- `PressureNodeLoRaProtocol`: compact FPort 30 command and FPort 31 status
  payload codec;
- RadioLib 7.7.1 integration: Class C continuous receive, low-power Class A
  command windows, OTAA/session retention, periodic status, immediate command
  result uplinks, and duplicate command rejection.

## Energy policy

The explicit runtime policies are:

- `pcv_serial_only`: ESP32 awake, Serial command input, radio disabled;
- `pcv_hybrid_class_c`: ESP32 awake, Serial command input, continuous Class C
  receive, and periodic status every 60-86400 seconds;
- `pcv_low_power_class_a`: Serial output only, one Class A status/command/result
  cycle, then ESP32 timer deep sleep. valve_1 commissioning temporarily allows
  10-86400 seconds and defaults to 10 seconds; restore a minimum of at least 60
  seconds before deployment;
- no automatic two-second status polling;
- the solenoid and L298N power rail are off outside an actuation pulse;
- TUF-2000M reads occur only after `flow`, `flow total`, `status`, a total
  reset, or one LoRaWAN report;
- RS-485 uses UART2 GPIO16/GPIO17 and confirmed unit address 1; ordinary reads
  use the hardware-confirmed `LOW_WORD_FIRST` decoder, while `flow probe`
  remains available for diagnostics;
- periodic sample and telemetry intervals are centralized but disabled;
- modes are represented as `Active`, `Idle`, and `SleepReady`; only the Class A
  target may reach `SleepReady`;
- LoRaWAN session state is mirrored to RTC memory and security nonces remain in
  NVS.

Hardware that cannot be disabled by this firmware must be included in the
measured energy budget: DevKit/regulator quiescent current and LEDs, the
passive battery divider, sensor boards and pull-ups, radio module, RS-485
converter, and any continuously powered flow meter. Measure sleep, idle,
sensing, radio, RS-485, and PCV-pulse current on the assembled node before
setting deployment intervals.

## TUF-2000M status

The supplied technical manual confirms function 03, zero-based Modbus start
addresses, factory 9600 8N1 framing, REG0001-0002 flow rate in m3/h,
REG0005-0006 velocity in m/s, and REG0072 error bits. These reads and the
manual's example-frame CRC are implemented and covered by compile tests.

GPIO16/GPIO17 are assigned to UART2 for the automatic-direction RS-485
converter. M46 address 1, M62 9600 8N1, and M63 MODBUS_RTU are confirmed. The
commissioned device's REAL4 layout is hardware-confirmed as `LOW_WORD_FIRST`.
Normal readings are enabled; `flow probe` reports both decodings for
diagnostics. Remaining validation items are:

1. device label/nameplate and firmware/ESN information;
2. actual 12 V rail voltage/current and meter power-up stabilization time; the
   supplied manual permits 8-36 VDC at approximately 50 mA;
3. power-cycling behavior and the final production termination/reference
   arrangement.

REG0113-REG0118 are implemented as direct `LOW_WORD_FIRST` REAL4 cubic-metre
net, positive, and negative totals. The user-confirmed ESP32 link reads live
flow and velocity successfully. `flow total reset` stores the current positive
total as a persistent ESP32 baseline; it does not modify the TUF accumulators.

Detailed confirmed facts and preserved references are in
`TUF_2000M_TS2.md`.

## Production PCB migration boundary

The present implementation is a one-valve prototype. The planned production
board provides two independent latching-valve channels and selected-at-a-time
protected RS485 branches, but this does not mean the current firmware already
supports two valves or RS485 pressure sensors. The production architecture,
provisional ESP32-S3 pinout, power profiles and release blockers are documented
in `UNIVERSAL_12V_CONTROLLER_PCB_TECHNICAL_SPEC_DRAFT.md`.

The safe bridge sequence in the current driver remains the behavioral basis
for each production channel. A later implementation shall instantiate two
independent valve channels, serialize pulses by default, and preserve the
confirmed TUF-2000M protocol facts. Existing prototype GPIO assignments remain
unchanged until a new production-board firmware target is explicitly created.

## Hardware validation checklist

- confirm GPIO27 active level against the BJT/P-MOSFET circuit;
- keep and verify the installed 20 kOhm pull-downs on GPIO2 and GPIO15, and fit
  the required pull-down on GPIO27 power-gate control so reset cannot energize
  the bridge;
- confirm OPEN and CLOSE polarity on the installed PCV;
- determine minimum reliable pulse duration and measure pulse current;
- validate XDB401 address, registers, full scale, sign, and temperature formula;
- calibrate battery voltage against a multimeter;
- confirm the physical lower divider resistor; the current 100 kOhm / 20 kOhm
  configuration and 0.9883 factor use the measured 12.435 V / 2.097 V point;
- provide the battery chemistry/capacity/discharge data before implementing
  SOC;
- define when hydraulic conditions allow pressure-based PCV verification;
- diagnose the upstream sensor's current -10 bar / -10 degrees C output before
  using either pressure difference or gateway inference;
- provision unique OTAA credentials and verify the ChirpStack FPort 30/31 codec;
- select an acceptable Class A sleep interval/command latency or Class C energy
  budget;
- measure the complete node energy budget in all operating states.
