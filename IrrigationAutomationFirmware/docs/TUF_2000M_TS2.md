# TUF-2000M with TS-2 clamp-on transducers

## Confirmed hardware and purpose

- Converter: TUF-2000M module type.
- Transducers: TS-2 (small), clamp-on.
- Documented pipe-size range: DN25-100.
- Documented transducer temperature range: -30 to 90 degrees C.
- Project purpose: obtain an on-demand water-flow measurement over RS485;
  continuous polling is not required.
- Installed pipe marking: PVC-U W/P, 63 x 3 mm, PN10.
- Commissioned hydraulic inputs: 63 mm outside diameter, 3 mm wall, 57 mm
  calculated inside diameter, water, no liner, and V-method.
- Reported M25 inner transducer spacing: 39.655 mm.

The manuals distinguish volumetric flow rate from fluid velocity. The firmware
therefore returns both values from one Modbus transaction:

- flow rate in cubic metres per hour;
- water velocity in metres per second.

## Confirmed RS485 and Modbus RTU protocol

The technical manual, section 7, confirms:

- isolated RS485 port;
- selectable Modbus ASCII or Modbus RTU in M63; the factory default is ASCII,
  so M63 must be changed to `MODBUS_RTU`;
- factory serial framing `9600, none, 8, 1`;
- programmable device address in M46;
- supported Modbus functions `03` (read registers) and `06` (write one
  register). The flow driver uses only read function `03`.

The manual's RTU example reads documented `REG0001` through `REG0010` from unit
1 with this request:

```text
01 03 00 00 00 0A C5 CD
```

This confirms that documented register numbers are one-based while the Modbus
start address is zero-based: `REG0001` is requested as address `0x0000`.
The installed meter's M46 address is confirmed as unit 1. Its M62 framing is
confirmed as 9600 none 8 1 and M63 is confirmed as `MODBUS_RTU`.

Registers used by the firmware:

| Manual register | Modbus start | Value | Format | Unit |
|---|---:|---|---|---|
| REG0001-0002 | `0x0000` | Flow rate | REAL4 | m3/h |
| REG0005-0006 | `0x0004` | Velocity | REAL4 | m/s |
| REG0072 | `0x0047` | Error code | 16 bits | bit field |
| REG0092 | `0x005B` | Working step / signal quality | 16 bits | Q in low byte, 0-99 |
| REG0093 | `0x005C` | Upstream strength | integer | 0-2047 |
| REG0094 | `0x005D` | Downstream strength | integer | 0-2047 |
| REG0097-0098 | `0x0060` | Measured/calculated travel-time ratio | REAL4 | normal 100 +/- 3% |
| REG0113-0114 | `0x0070` | Net accumulated volume | REAL4 | m3 |
| REG0115-0116 | `0x0072` | Positive accumulated volume | REAL4 | m3 |
| REG0117-0118 | `0x0074` | Negative accumulated volume | REAL4 | m3 |

## Raw Modbus RTU bench requests for unit 1

These complete hexadecimal frames include the Modbus CRC in wire order
(low byte first). A USB-RS485 program that appends CRC automatically must be
given only the first six bytes.

| Purpose | Complete request |
|---|---|
| Confirm REG1442 device address | `01 03 05 A1 00 01 D5 24` |
| Flow rate, REG0001-0002 | `01 03 00 00 00 02 C4 0B` |
| Velocity, REG0005-0006 | `01 03 00 04 00 02 85 CA` |
| Flow, energy flow, and velocity, REG0001-0006 | `01 03 00 00 00 06 C5 C8` |
| Error bits, REG0072 | `01 03 00 47 00 01 34 1F` |
| Working step, Q, and signal strengths, REG0092-0094 | `01 03 00 5B 00 03 74 18` |
| M91 time ratio, REG0097-0098 | `01 03 00 60 00 02 C4 15` |
| Net, positive, negative totals, REG0113-0118 | `01 03 00 70 00 06 C4 13` |
| Inner diameter, REG0221-0222 | `01 03 00 DC 00 02 05 F1` |

For the confirmed address 1, the expected REG1442 response is
`01 03 02 00 01 79 84`. This integer-register exchange is the preferred first
link test because no REAL4 word-order decision is involved. The combined
REG0001-REG0006 request is the same transaction used by Serial `flow probe`.

The project also preserves a standalone Windows diagnostic utility at
`tools/USB_RS485_Sensor_Tool_v0_9_2/`. Its TUF profile performs all seven reads
above in one action, keeps each complete TX/RX frame visible, decodes the M08
error bits, M90 diagnostics, and all three volume accumulators, and decodes
REAL4 as the hardware-confirmed `LOW_WORD_FIRST` layout. The packaged
executable and ZIP are under that tool's `release/` directory.

The on-demand `flow` read sends two function-03 transactions:

1. read `REG0001-REG0006` once and decode flow rate plus velocity, ignoring the
   intervening energy-flow value;
2. read `REG0072` once and reject a sample when its flow-relevant error bits
   report missing/poor signal, empty pipe, hardware/checksum/clock/parameter
   faults, gain adjustment, or internal timer overflow.

Output-only and energy/analog-input-only error bits remain available in the
returned diagnostic field but do not by themselves invalidate a flow-only
sample.

## Hardware-confirmed REAL4 word order

The manual defines `REAL4` as a single IEEE-754 32-bit float. It does not state
whether the first Modbus register contains the high or low 16-bit word. Modbus
itself does not standardize multi-register word order.

The commissioned meter returned raw REG0221-REG0222 data `00 00 42 64`. With
the configured inner diameter known to be 57.0 mm, the only valid decoding is
to swap the two 16-bit register words first, producing IEEE-754 bytes
`42 64 00 00` = 57.0. This hardware result confirms `LOW_WORD_FIRST` for the
commissioned device.

The firmware therefore enables normal flow decoding with `LOW_WORD_FIRST`.
The explicit Serial `flow probe` command still prints the raw bytes and both
interpretations so future replacement meters can be diagnosed without hiding
their wire format.

An earlier hardware read showed M08 error bit value 4 (`poor received signal`),
signal quality Q = 2, and M91 time ratio about 77.56 percent. RS485
communication and decoding were already working at that point. The user has
since confirmed that both the meter and ESP32 show live velocity and flow.

## Accumulated water volume and safe reset

The manual provides direct cubic-metre REAL4 totals at REG0113-REG0118. The
firmware reads all three in one function-03 transaction:

- net volume;
- positive volume;
- negative volume.

Delivered water since reset is based on the positive accumulator. The command
`flow total reset` stores the current positive total as an ESP32 baseline in
NVS. Later values are calculated as `current positive - saved baseline`, so a
reset produces 0 without altering the meter. The baseline survives ESP32
restart and Class A deep sleep.

This distinction is intentional. The manual documents totalizer reset through
interactive menu M37, but the Modbus table does not define a direct totalizer
reset register. M37 can also enter a master erase sequence. The firmware and
diagnostic utility therefore remain read-only toward TUF totalizers and do not
simulate menu-key writes.

## Confirmed terminals and menus

The TUF-2000M manual specifies an `8-36 VDC` supply at approximately 50 mA
(`10-36 VAC` is also stated). Therefore the installed 12 V battery is within
the documented DC input range; 24 V is a common nominal supply, not a minimum
requirement. Confirm the actual voltage at the meter terminals under load and
follow the installed unit's nameplate if it differs from the supplied manual.

The TUF-2000M module wiring diagram shows:

- `8-36V+` / `8-36V-`: power (some drawings or accessories use a nominal
  `24V` label);
- `485+` / `485-`: RS485;
- `UP+` / `UP-` / `GND`: upstream ultrasonic transducer;
- `DN+` / `DN-` / `GND`: downstream ultrasonic transducer;
- additional analog, OCT, relay, and temperature terminals that are not needed
  for this flow-only integration.

Relevant menus:

- M00: flow rate and net totalizer;
- M01: flow rate and velocity;
- M08: system error code (`R` means normal in the general manual);
- M11-M25: pipe, lining, liquid, transducer, mounting, and spacing parameters;
- M26: store current parameters in flash for reuse after power-up;
- M40: damping, 0-999 seconds, factory default 10 seconds;
- M41: low-flow cutoff;
- M46: network address/ID;
- M49: display serial-port input for communication checking;
- M62: RS232/RS485 serial configuration;
- M63: communication protocol;
- M90: upstream/downstream signal strength and Q value;
- M91: measured/calculated transit-time ratio; installation guidance gives an
  acceptable range of 97-103 percent.

## Remaining activation data

The register map is implemented. M46 address 1, M62 9600 none 8 1, M63
MODBUS_RTU, `LOW_WORD_FIRST` decoding, live flow/velocity, and the physical
ESP32 RS485 link are commissioned. Normal reads, accumulated-volume reads, and
`flow probe` are enabled. The following still require validation:

- measured power-on stabilization time and whether power cycling the meter is
  acceptable.

These settings are centralized in `include/PressureNodeConfig.h`.

## On-demand power strategy

The supplied manual explicitly permits 8-36 VDC, so the meter can be supplied
from a suitably protected and switched 12 V rail without a boost converter.
The rail still needs correct polarity, enough current, suitable protection,
and a measured voltage at the meter terminals. The installed unit's nameplate
takes precedence if it states a narrower range.

Suggested measurement cycle after bench validation:

```text
ESP32 wakes or receives a flow request
  -> enable the protected meter supply and RS485 transceiver
  -> wait for the measured/confirmed TUF-2000M stabilization time
  -> read flow/velocity and the error register once
  -> report values, units, diagnostics, age, and validity
  -> disable meter/transceiver power if validated for power cycling
  -> return to idle or sleep-ready state
```

M26 indicates that commissioned parameters can be stored in flash and reloaded
after power-up, but power cycling, stabilization time, and measurement quality
must be verified on the actual meter before adopting this strategy.

## Installation checks to retain

- pipe must remain full at the measurement point;
- avoid vibration, strong EMI, and disturbed flow where possible;
- clean the pipe surface and use suitable acoustic couplant;
- enter actual pipe and liquid parameters before measuring;
- verify signal strength/Q in M90;
- verify the M91 ratio is 97-103 percent;
- store the accepted configuration with M26.

## Preserved sources

- `references/tuf2000m_ts2/TUF-2000M_Technical_Manual_Modbus.pdf`
- `references/tuf2000m_ts2/TUF-2000_Series_Ultrasonic_Flowmeter_User_Manual.pdf`
- `references/tuf2000m_ts2/TUF-2000M_module.png`
- `references/tuf2000m_ts2/TS-2_range.png`
- `references/tuf2000m_ts2/TUF-2000M_TS-2_clamp_on.png`

SHA-256 hashes:

- technical Modbus manual:
  `f82c2d7ecf69bdf44d5a3e7f6a899d9a836df30a50906a7603e94c5e5932d801`;
- general user manual:
  `dc84b421a02ae5d5c55157acd946d6f283d4400f6e51d392565478be754f7029`.
