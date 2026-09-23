# EPEVER LandStar LS1024B solar charge controller

The LS1024B is a PWM solar charge controller with an RS-485 Modbus RTU port. It
powers itself from the battery, so the firmware never switches it on or off; the
node only owns the RS-485 path.

Driver: `lib/SolarController_EP_LS1024B/`. Bench console:
`lib/SolarControllerTestConsole/`, linked by the `pcv_solar_test` PlatformIO
target. The console takes the battery profile from its caller, so the profile
stays an installation value in the node's own configuration.

## Commissioned device

| Item | Value | Status |
| --- | --- | --- |
| Modbus RTU slave address | 0x60 (96) | set with the SolarGuardian PC tool |
| Framing | 115200 8N1 | from the commissioning sketch |
| Real-time values | FC04 input registers | battery voltage and temperature already read on this unit |
| Charge setpoints | FC03 read, **FC10 block write** of 0x9003..0x900E | method taken from the LS1024B configuration sketch; map **not confirmed against an LS1024B document** |
| Address change | proprietary service command `0x45` | captured from the official PC tool |

The controller powers itself from the battery, and its **LOAD output is
battery-tracking, not a regulated 14.4 V supply**. Anything connected to LOAD
must tolerate the full charge voltage; use a DC-DC converter where the load's
input ceiling is 14.4 V. No firmware on this node can clamp that.

## Where the register map comes from

Nothing in the driver is inferred from the device name. Each address has a
source, and the source quality is recorded in the code:

- **Real-time block (0x3100-0x3111, 0x311A, 0x3200-0x3201)** — from the
  commissioned LS1024B sketches. Battery voltage (0x3104), battery temperature
  (0x3110), load current (0x310D), and battery status (0x3200) are proven on the
  installed controller; the rest of the map is read from the same source and
  cross-checked against the controller display. Flagged
  `REALTIME_BLOCK_HARDWARE_CONFIRMED = false`.
- **Setting registers (0x9000-0x900E, 0x903D, 0x9067, 0x90BF)** — from those
  sketches and the EPEVER Tracer-AN G3 protocol table. Flagged
  `SETTINGS_MAP_HARDWARE_CONFIRMED = false`.
- **Service command 0x45** — captured from the official PC tool, exactly as
  already recorded in `tools/USB_RS485_Sensor_Tool_v0_9_2/` (see its
  `README_EN.md` and `device_profiles.json`).

### Real-time registers (FC04)

| Register | Meaning | Scale |
| --- | --- | --- |
| 0x3100 / 0x3101 | PV input voltage / current | 0.01 V / 0.01 A |
| 0x3104 / 0x3105 | Battery voltage / charging current | 0.01 V / 0.01 A |
| 0x310C / 0x310D | Load voltage / current | 0.01 V / 0.01 A |
| 0x3110 / 0x3111 | Battery / device temperature | signed, 0.01 C |
| 0x311A | Battery state of charge | 1 % |
| 0x3200 / 0x3201 | Battery status / charging status | bitfield, undecoded |

Read as four transfers, each within a twelve-register envelope: 0x3100 ×12,
0x310C ×6, 0x311A ×1, 0x3200 ×2.

### Setting registers (FC03 read)

| Register | Meaning | Scale |
| --- | --- | --- |
| 0x9000 | Battery type (0 user, 1 AGM, 2 GEL, 3 flooded) | code, **read-only here** |
| 0x9001 | Battery capacity | Ah, **read-only here** |
| 0x9002 | Temperature compensation coefficient | **read-only here** |
| 0x9003 | Over-voltage disconnect voltage | 0.01 V, **written** |
| 0x9004 | Charging limit voltage | 0.01 V, **written** |
| 0x9005 | Over-voltage reconnect voltage | 0.01 V, **written** |
| 0x9006 | Equalize charging voltage | 0.01 V, **written** |
| 0x9007 | Boost charging voltage | 0.01 V, **written** |
| 0x9008 | Float charging voltage | 0.01 V, **written** |
| 0x9009 | Boost reconnect voltage | 0.01 V, **written** |
| 0x900A | Low-voltage reconnect voltage | 0.01 V, **written** |
| 0x900B | Under-voltage recover voltage | 0.01 V, **written** |
| 0x900C | Under-voltage warning voltage | 0.01 V, **written** |
| 0x900D | Low-voltage disconnect voltage | 0.01 V, **written** |
| 0x900E | Discharging limit voltage | 0.01 V, **written** |
| 0x903D | Load control mode | code, may not answer, **read-only here** |
| 0x9067 | Rated voltage level (0 auto, 1 12 V, 2 24 V) | code, **read-only here** |
| 0x90BF | Maximum charging current | 0.01 A, **read-only here** |

## Writing the charge setpoints

`0x9003..0x900E` is written as **one FC10 block request**, because the twelve
setpoints are mutually constrained and must change together. Function 0x06
single-register writes are not used anywhere in this driver.

Before anything is sent, three checks must pass:

1. **Local ordering rule.** The block must satisfy the ordering predicate the
   configuration sketch defines, kept verbatim as
   `epever_ls1024b::protocol::voltageBlockOrdered()`. It is checked in the driver
   *and* asserted at compile time against the configured profile, so an edit to
   the thresholds cannot reach the bench. Failing it reports `SETPOINTS_INVALID`
   and sends nothing.
2. **Battery type** must already be `0` (user-defined) on the controller.
3. **Rated voltage level** (`0x9067`) must already be `1` (12 V).

Checks 2 and 3 read the controller and abort with `PRECONDITION_FAILED` on any
mismatch — the setpoints mean "user-defined 12 V battery" setpoints, so writing
them onto a factory battery type or a 24 V system would be wrong.

If the block already matches, nothing is written at all: the settings live in
EEPROM, and rewriting identical values is wear without benefit. Otherwise the
block is written, the controller is given 250 ms, and the **whole block is read
back**. `APPLIED` requires every one of the twelve to read back as sent;
otherwise the report says `VERIFICATION_MISMATCH` and the stored settings must be
treated as uncertain.

Battery type, capacity, temperature compensation, rated voltage level, and
maximum charging current are **not written by this firmware**. Set them on the
controller itself.

## Address change

The controller address is changed with a proprietary EPEVER service command, not
with Modbus:

```text
Find ID: F8 45 00 01 01 F8 + CRC   ->  F8 45 00 01 01 F8 89 BE
Set ID:  F8 45 00 01 01 NN + CRC   ->  F8 45 00 01 01 60 88 14   (new address 0x60)
```

Function `0x45` is not a Modbus function, the frame is broadcast (address
`0xF8`), and the response format is undocumented. The driver therefore records
any response byte-for-byte and never parses it — the same behaviour as the PC
tool. Both frames are asserted byte-for-byte against these captured examples in
`test/test_protocol/test_main.cpp`.

Because the frame is a broadcast, **exactly one controller may be connected to
the trunk** while an address change is being sent. Success is decided the way
the PC tool decides it: after a settle delay, the new address must answer a
battery-voltage read and the previous address must stop answering.

## Bench procedure

Only one controller on the trunk, Serial Monitor at 115200 baud:

```text
pio run -e pcv_solar_test -t upload

solar                  # live PV/battery/load/temperature/SOC
solar settings         # read the settings area, no writes
solar profile          # print the 12 setpoints and the ordering verdict
solar write confirm    # FC10 block write, then read the whole block back
solar settings         # confirm independently
solar find             # print the raw service-command response
solar address 0x4E confirm
```

`pcv_solar_test_readonly` is the same firmware with the writes and the address
change compiled out; `solar write confirm` must answer `WRITES_DISABLED` and put
nothing on the wire.

Compare `solar settings` against the controller's own display before writing
anything. That comparison is what confirms or refutes the 0x9000 base address on
this device, and no read-back can detect a wrong register if the controller
happily stores whatever it is sent.

After a confirmed address change, update `SLAVE_ADDRESS` in
`examples/PressureControlNode/include/PressureNodeConfig.h` and rebuild. The
firmware cannot discover an address it does not know.

## UART2 is shared with the TUF-2000M

GPIO16/17 is the node's single RS-485 branch. The TUF-2000M runs at 9600 baud
and the LS1024B at 115200, so the two cannot share it. The `pcv_solar_test`
target compiles the flow meter out (`PCV_NO_FLOW_METER`) and gives the branch to
the solar controller; every other `pcv_*` target keeps the flow meter.

The PCV valve, both pressure sensors, the battery monitor, and the LoRaWAN
protocol are unaffected in the solar test build.

## Open items

- The settings map is unconfirmed against an LS1024B document; the bench
  read-back and the comparison with the controller display are the experiments
  that settle it.
- The reported temperature-compensation value (0x9002) is shown as a raw word.
  It is never written, so its sign and encoding no longer need to be trusted.
- Equalization and boost duration are not written: no register address for them
  is documented by any source in this repository for this device family.
- PV, battery, and load power (0x3102/0x3103, 0x3106/0x3107, 0x310E/0x310F) are
  not decoded. Their 32-bit L/H word order is undocumented here.
- 0x3200/0x3201 status bits are reported raw and not interpreted.
- The twelve-register read envelope is a property of the reference
  implementation, not a documented device limit. If longer reads turn out to
  work, the block sizes could grow.
