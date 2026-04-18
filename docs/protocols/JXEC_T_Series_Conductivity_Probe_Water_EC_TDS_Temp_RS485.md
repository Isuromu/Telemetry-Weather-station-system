# JXEC-T Series Water Conductivity Controller + Probe (RS485)

Source: `Conductive probe instruction of water sensor.pdf`, plus vendor protocol notes for the JXCT water conductivity controller used with a JXEC-T probe.

## 1) Overview
- Measures water temperature and electrical conductivity.
- The metal JXEC-T probe alone is not necessarily a Modbus device. It must be connected to the matching transmitter/controller assembly that exposes RS485/Modbus.
- If only the bare probe is connected to A/B, there will be no Modbus response.

## 2) Electrical & RS485 settings
- Protocol: Modbus RTU
- Read function: `0x03`
- Default Modbus address: `0x01`
- Default baud: `9600`
- Serial format: 8 data bits, no parity, 1 stop bit
- CRC: Modbus CRC16, low byte first on the wire
- Data byte order: high byte first inside register payload

Typical RS485 controller wiring:
- Brown: V+
- Black: GND
- Yellow/Grey: 485-A
- Blue: 485-B

## 3) Register map
| Register | Access | Meaning | Scaling / notes |
|---:|---|---|---|
| `0x0001` | Read | Water temperature | raw / 10.0 = deg C |
| `0x0002` | Read | Conductivity high word | upper 16 bits of raw u32 |
| `0x0003` | Read | Conductivity low word | lower 16 bits of raw u32 |
| `0x0100` | Read/write | Device address | Modbus slave address |
| `0x0101` | Read/write | Baud rate | vendor-specific encoding |

Conductivity is reported as a 32-bit raw value:

```text
conductivity_raw = (reg0002 << 16) | reg0003
```

For the documented K=1 example:

```text
conductivity_uS_cm = conductivity_raw / 100.0
```

The raw format is shared, but final engineering interpretation depends on the probe/controller range and cell constant.

## 4) Read commands
### Temperature only
Request for address `0x01`:

```text
01 03 00 01 00 01 D5 CA
```

Response layout:

```text
01 03 02 TT_H TT_L CRC_L CRC_H
```

Decode:

```text
temp_raw = (TT_H << 8) | TT_L
temperature_C = temp_raw / 10.0
```

Example: `0x00AF = 175 -> 17.5 C`.

### Conductivity only
Request for address `0x01`:

```text
01 03 00 02 00 02 65 CB
```

Response layout:

```text
01 03 04 EC3 EC2 EC1 EC0 CRC_L CRC_H
```

Decode:

```text
conductivity_raw = (EC3 << 24) | (EC2 << 16) | (EC1 << 8) | EC0
```

Example: `00 00 00 BD = 189`, K=1 example -> `1.89 uS/cm`.

### Temperature + conductivity
Request for address `0x01`:

```text
01 03 00 01 00 03 54 0B
```

Response layout:

```text
01 03 06 TT_H TT_L EC3 EC2 EC1 EC0 CRC_L CRC_H
```

Decode:

```text
temperature_C = ((TT_H << 8) | TT_L) / 10.0
conductivity_raw = (EC3 << 24) | (EC2 << 16) | (EC1 << 8) | EC0
```

Example: `0x011B = 283 -> 28.3 C`, `0x00000028 = 40`, K=1 example -> `0.4 uS/cm`.

## 5) Write commands
- Address register: `0x0100`
- Baud register: `0x0101`

Use write commands only with one target controller on the RS485 bus. The driver implements address change with function `0x06` to register `0x0100`; baud changing is intentionally not implemented until the exact baud encoding is confirmed on the hardware variant.

## 6) Range protection & fault detection
Driver defaults:
- Temperature accepted range: `-10..80 C`
- Conductivity accepted range: `0..200000 uS/cm` by default, configurable per probe/controller range
- Fault raw patterns rejected: `0xFFFFFFFF`, `0x7FFFFFFF`, `0x80000000`

## 7) Practical pitfalls
- Bare probe only: no Modbus response.
- Analog-output variant: no RS485 response.
- Wrong A/B polarity: no response.
- Wrong conductivity scale: CRC-valid frame but incorrect engineering value.
- After power cycling, stainless probes may report wrong values if water remains on the electrode surface; verify against the manual and real installation behavior.
