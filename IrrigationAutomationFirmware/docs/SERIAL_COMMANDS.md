# Serial command reference

Open the Serial Monitor at 115200 baud with newline line endings. Commands are
lowercase and whitespace-separated.

## Pump

```text
pump freq 10
pump freq 25
pump freq 42.5
pump freq 50
pump start
pump stop
pump estop
pump status
pump telemetry
pump fault
pump reset
```

`pump start` is rejected until a valid frequency has been sent after the latest
boot. Allowed frequency is 10..50 Hz. Reverse is not a command.

`pump stop` uses CDI-E command `0x0006`, deceleration stop. `pump estop` uses
`0x0005`, free stop, and clears the frequency arm so another explicit `pump
freq` command is required before restart.

## VFD

```text
vfd ping
vfd info
vfd config
vfd config check
vfd config apply CONFIRM
vfd read 0xB000
vfd read 0xB001
vfd param read P0.0.17
vfd param read P4.1.02
```

`vfd config` and `vfd config check` are identical read-only checks. No parameter
is changed. Configuration application requires the exact confirmation word and
is refused while the VFD reports a running state.

There is no unrestricted `vfd param write` command.

## Modbus debug

```text
modbus debug on
modbus debug off
```

Debug output includes TX/RX frames, CRC result, response size, address, timeout,
exception, and CDI-E device error information. Compile-time default is
controlled by `DEBUG_MODBUS` in `platformio.ini`.

## Raw diagnostics

The convenient form appends CRC automatically:

```text
modbus raw 01 03 B0 00 00 01
```

This reads one register from `0xB000`. Hex bytes do not use the `0x` prefix.

To send a complete frame with an existing CRC:

```text
modbus rawcrc 01 03 B0 00 00 01 <CRC-low> <CRC-high>
```

Raw reads are enabled. Raw write functions are disabled by default with
`ENABLE_DANGEROUS_RAW_WRITES=0`. If an engineer deliberately enables that build
flag, every raw write must still end with `CONFIRM_WRITE`.

## Pressure-control end node

Build and upload `pcv_serial_only` or `pcv_hybrid_class_c`, then use:

```text
help
status
battery
pressure
flow
flow total
flow total reset
flow probe
pcv open
pcv close
pcv state
```

`pcv state` distinguishes the last command, verified/inferred state, and
verification status. A successful local or LoRaWAN command is retained across
sleep/reset as the last commanded state. The current hardware has no direct
valve-position feedback, so the physical state remains `UNKNOWN` and a
successful pulse never produces a false `VERIFIED` result.

The `pcv_low_power_class_a` target prints diagnostics while it is awake but
deliberately does not parse Serial commands.

`battery` reports the measured GPIO35 voltage and calibrated battery voltage,
and leaves SOC as `UNKNOWN`.
`pressure` marks the current engineering scale `VALID_UNCALIBRATED`.
`flow` is an on-demand TUF-2000M read. When commissioned, it reports flow rate
in m3/h, velocity in m/s, and REG0072 error bits. UART2 is assigned to
GPIO16/GPIO17; M46 address 1, M62 9600 8N1, and M63 MODBUS_RTU are confirmed.
The commissioned device's REAL4 layout is hardware-confirmed as
`LOW_WORD_FIRST`, so normal `flow` is enabled. `flow probe` prints the raw
REG0001-REG0006 response plus both interpretations as a diagnostic.

`flow total` reads REG0113-REG0118 and prints:

- delivered water since the last ESP32 baseline reset in m3 and litres;
- the TUF net, positive, and negative accumulators in m3.

Before the first reset, the raw meter totals are still available and the
since-reset value is `NOT_SET`. `flow total reset` reads the current positive
accumulator and stores it as the local zero point in NVS. It does not erase the
TUF-2000M internal accumulators. `flow reset` is accepted as a short alias.
