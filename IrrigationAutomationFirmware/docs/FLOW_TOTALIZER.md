# TUF-2000M accumulated water volume

## What each value means

The pressure-node firmware keeps three different hydraulic quantities
separate:

| Quantity | Source | Unit | Meaning |
|---|---|---|---|
| Flow rate | REG0001-REG0002 | m3/h | Instantaneous volumetric flow |
| Water velocity | REG0005-REG0006 | m/s | Instantaneous pipe velocity |
| Accumulated volume | REG0113-REG0118 | m3 | Water counted over time |

The accumulated block contains net, positive, and negative totals as direct
REAL4 cubic-metre values. The commissioned meter uses `LOW_WORD_FIRST`.

## Serial commands

```text
flow
flow total
flow total reset
```

`flow` reads instantaneous rate, velocity, and M08 error bits.

`flow total` prints the TUF net, positive, and negative totals. After a local
reset it also prints delivered water since reset in cubic metres and litres.

`flow total reset` reads the current positive total and stores it as the ESP32
baseline. The short alias `flow reset` is also accepted.

## Reset safety and persistence

The reset is deliberately local:

```text
since_reset_m3 = current_positive_total_m3 - saved_positive_baseline_m3
```

The baseline is stored with the other node state in NVS and retained in RTC
memory during Class A deep sleep. It therefore survives reboot and sleep.

No totalizer write is sent to the TUF-2000M. The manual describes reset only
through interactive M37 and does not assign a direct Modbus reset register.
M37 also exposes a master-erase sequence, so simulating its keypad operations
is outside the safe read-only driver.

If somebody resets or replaces the meter externally and its positive total
becomes smaller than the saved baseline, the firmware does not report a
negative consumption value. Raw meter totals remain visible and
`flow total reset` must be issued again.

## Modbus transaction

For commissioned unit 1, the complete RTU request is:

```text
01 03 00 70 00 06 C4 13
```

It requests documented REG0113-REG0118 using zero-based start address
`0x0070`. The 12 response data bytes are decoded as three consecutive
`LOW_WORD_FIRST` REAL4 values.

## ChirpStack

Both LoRaWAN codecs use protocol v2. Reset with a new command ID:

```json
{"flow_total_reset":true,"command_id":3001}
```

The FPort 31 status exposes:

- `flow_total_since_reset_m3`;
- `flow_total_since_reset_liters`.

The value is transmitted as unsigned litres. `0xFFFFFFFF` means that no
local baseline is available. An exact duplicate command is acknowledged but
does not move the baseline again.
