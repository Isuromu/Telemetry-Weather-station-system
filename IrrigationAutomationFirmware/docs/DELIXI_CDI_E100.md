# DELIXI CDI-E100 Modbus driver

Primary source: DELIXI CDI-E frequency inverter manual, especially Chapter 8
and parameter tables P0, P4.1, and P9.0.

## Physical unit

- Model: CDI-E100G2R2T4B
- Input: three-phase 380 V, 50/60 Hz
- Output: three-phase 0..380 V
- VFD rating: 2.2 kW, 6.0 A
- Protection: IP20

The motor protection limit is based on the connected motor's 3.3 A rating, not
the VFD's 6.0 A rating.

## Initial communication

| Parameter | Expected value | Meaning |
|---|---:|---|
| P4.1.00 | 3 | 9600 baud |
| P4.1.01 | 3 | 8N1, matching the commissioned physical VFD |
| P4.1.02 | 1 | slave address 1 |
| P4.1.03 | 2 | 2 ms response delay |
| P4.1.04 | 0 | timeout disabled for first bring-up |
| P4.1.05 | 1 | RTU mode |
| P4.1.06 | 0 | replies enabled |

P4.1.04 must be changed to a tested non-zero value for final deployment. The
manual states that an elapsed communication timeout produces fault Err14. The
installation must also verify the configured fault action and must not rely on
firmware alone for emergency protection.

## Verified register map

| Address | Access | Scale or value |
|---:|---|---|
| 0xA000 | write | run/stop/reset command |
| 0xA001 | write | 0..10000 = 0.00..100.00% of maximum frequency |
| 0xB000 | read | 1 forward, 2 reverse, 3 stopped |
| 0xB001 | read | CDI-E fault number |
| 0x9000 | read | output frequency, 0.01 Hz |
| 0x9001 | read | reference frequency, 0.01 Hz |
| 0x9002 | read | output current, 0.01 A |
| 0x9003 | read | output voltage, 1 V |
| 0x901B | read | communication set value, 0.01% |

`0x901B` corresponds to P9.0.27: decimal parameter index 27 becomes hexadecimal
low byte `0x1B`.

## Commands written to 0xA000

| Value | Manual definition | Project use |
|---:|---|---|
| 0x0001 | forward run | pump start |
| 0x0002 | reverse run | driver definition only; not exposed by PumpController |
| 0x0003 | forward jog | not exposed |
| 0x0004 | reverse jog | not exposed |
| 0x0005 | free stop | `pump estop` |
| 0x0006 | deceleration stop | `pump stop` and boot stop policy |
| 0x0007 | fault reset | `pump reset` |

## Function codes and CRC

- `0x03`: read one or more registers
- `0x06`: write one register
- CRC-16/MODBUS is sent low byte first

The transport validates CRC, address, function, standard Modbus exceptions,
and the CDI-E manual's `FF01 + error code` response form.

## Parameter addressing

The high address byte is formed from the parameter group and level. The low
byte is the decimal parameter index converted to hexadecimal.

Examples from the manual rule:

```text
P2.1.12 persistent address = 0x210C
P2.1.12 RAM-only address   = 0x250C
P4.1.02 persistent address = 0x4102
P0.0.17 persistent address = 0x0011
```

The manual's access summary says P1..P8 are readable/writable even though its
configuration examples and parameter map also require P0 values. This driver
uses the same documented addressing rule for P0 and records this manual wording
as an ambiguity rather than inventing a different map.

## Configuration behavior

`checkConfiguration()` is read-only. It checks control mode, frequency source,
frequency limits, ramp times, motor nameplate values, V/F mode, communication
settings, and that parameter identification is idle.

`applyConfiguration()`:

- must be called explicitly through `vfd config apply CONFIRM`;
- refuses to run unless B000 reports stopped;
- writes only mismatched persistent parameters;
- does not overwrite an existing non-zero communication timeout with the
  bring-up profile's disabled value;
- never starts motor identification;
- never runs automatically at boot.
