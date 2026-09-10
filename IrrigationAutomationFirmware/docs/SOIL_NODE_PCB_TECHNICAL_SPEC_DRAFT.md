# AmudarIO solar soil node PCB technical specification - review draft

> Latest hardware requirements supersede this review: mandatory LTC4121-4.2
> MPPT, MAX-M10S GPS/GNSS and C6-P2. Use
> `AmudarIO_Soil_Node_PCB_Design_Requirements_v1_2_EN.md` and
> `SOIL_NODE_ESP32C6_PINOUT_C6_P2_EN.md`. Older GNSS exclusions, LTC4079 and
> C6-P1 instructions below are retained only as historical review material.

## 1. Document status

Document revision: `Draft 0.5`

Updated: 2026-09-04. Full GPIO/module-pad table is in Section 11.
Russian standalone review copy:
[SOIL_NODE_ESP32C6_PINOUT_RU.md](SOIL_NODE_ESP32C6_PINOUT_RU.md).

Purpose: architecture review and owner approval before schematic capture.

This document is not an approved schematic, released BOM, purchasing list, or
manufacturing package. Parts marked `PROPOSED` are engineering recommendations.
Values marked `TBD` must not be invented by the PCB contractor.

The source HRS documents are technical references, not instructions that can
override the current user request or later project decisions:

- [soil-node HRS](references/hardware_requirements/Hardware_Requirements_AmudarIO_Soil_Node.pdf);
- [controller/hub HRS](references/hardware_requirements/Hardware_Requirements_Irrigation_Controller_Hub.pdf).

The controller/hub HRS describes the future valve and flow controller board.
Its ESP32-S3, 12 V lead-acid input, two H-bridges, four isolated RS485 branches,
flow-meter ports and 50 uA standby target are out of scope for this soil node.

Requirement labels used below:

- `REQ` - retained requirement from the soil HRS or current user decision;
- `PROPOSED` - recommended baseline for approval;
- `TBD` - release input that is still missing;
- `EXCLUDED` - intentionally omitted from this first soil-node revision.

## 2. Product scope

### 2.1 Intended product

`REQ`: one compact autonomous field node that periodically reads one external
5 V RS485 soil probe and sends the result to ChirpStack over LoRaWAN Class A.

The baseline measurement set is:

- soil temperature;
- volumetric water content;
- electrical conductivity;
- node battery voltage;
- explicit sensor and node diagnostic status.

The exact probe model, Modbus register map, startup time and current are `TBD`.
The PCB shall not encode undocumented sensor assumptions.

### 2.2 Normal operating cycle

```text
timer wake
  -> establish safe output states
  -> enable RS485 transceiver and 5 V sensor rail
  -> wait the validated sensor startup time
  -> perform bounded Modbus RTU read/retry sequence
  -> disable the complete sensor domain
  -> enable switched battery divider and measure VBAT
  -> disable battery divider
  -> transmit versioned LoRaWAN status uplink
  -> open RX1/RX2 windows
  -> validate and apply a queued command
  -> send an immediate application result uplink when required
  -> put DX-LR30 into software shutdown
  -> enter ESP32-C6 timer deep sleep
```

### 2.3 Baseline exclusions

The following functions are `EXCLUDED` from the first PCB BOM:

- GNSS/GPS;
- external RTC;
- ADS1115 or another external ADC;
- MAX17048-class fuel gauge;
- local data-logging memory beyond ESP32 flash/NVS and RTC/LP memory;
- Wi-Fi, Bluetooth and 802.15.4 operation in the deployed firmware;
- a second soil sensor port;
- valve, pump or flow-meter control;
- galvanically isolated multi-channel RS485 hub functions.

Network-server time is sufficient for an immediately transmitted measurement.
The ESP32 internal RTC timer is sufficient for the baseline wake interval. Its
interval drift across temperature must be characterized; this is not a promise
of calendar-grade timekeeping.

## 3. Requirements reconciliation

| Soil HRS item | Draft decision | Status |
|---|---|---|
| ESP32-C6 | ESP32-C6-MINI-1-H4 module, 4 MB flash | PROPOSED |
| SX1262 LoRa module | Existing DX-LR30-900M22S module | REQ, exact purchased revision TBD |
| 868 MHz / EU868 | Radio supports 850-930 MHz; deployed LoRaWAN region and antenna are TBD | TBD |
| SMA antenna connector | One edge-mounted LoRa SMA female connector | REQ |
| GPS/GNSS and second SMA | Not fitted | EXCLUDED by current simplification decision |
| 1S 3.7 V Li-ion | One removable cylindrical 18650 cell in a PCB-mounted holder sized for a protected cell | REQ, exact cell/holder TBD |
| Solar charger for nominal 6 V panel | LTC4079 with fixed input-voltage regulation set from the measured panel Vmp | PROPOSED, panel/thermal/energy validation required |
| Cell protection | Protected removable cell or dedicated ultra-low-IQ board circuit | REQ, implementation TBD |
| Charge temperature inhibit | Hardware NTC network matched to the selected cell | REQ |
| Mechanical system switch | Hard disconnect between protected battery node and all system loads; charger remains connected | REQ |
| Switched ADC divider | ESP32-C6 ADC with high-side low-leakage switch | REQ |
| Programming/debug | External 3.3 V USB-to-TTL on dedicated UART0 GPIO16/GPIO17; no on-board bridge or native USB connector | REQ |
| Fully gated 5 V probe rail | TPS61023 boost, default OFF | PROPOSED |
| RS485 with A/B protection | Switched THVD1406 AutoDirection plus SM712 and surge-rated series resistors | PROPOSED, low-baud validation required |
| Diagnostic LEDs | Optional cuttable/DNP charge LED only; no baseline user/power LED | PROPOSED |
| 2- or 4-layer PCB | Two layers are the cost baseline; four layers only if layout/EMC review requires them | PROPOSED |
| Complete-board timer deep sleep | <=25 uA in the defined acceptance setup | REQ |
| Any inactive/quiescent state | <1 mA with no intentional sensor, radio or charging activity | REQ |
| Contractor manufacturing package | Expanded deliverable list in Section 16 | REQ |

The LTC4079 is not a perturb-and-observe MPPT. It holds the panel near one
resistor-programmed input voltage selected from the measured Vmp. This gives a
simple, ultra-low-dark-current solar charger, but it is a linear charger: with
a 6 V panel and a roughly 4 V cell, its ideal power-efficiency ceiling is about
67 percent before other loss. The panel and winter energy budget therefore
remain release inputs.

### 3.1 Makerfabs reference-board audit

The Makerfabs `Industrial-grade-Soil-Remote-Monitor-V2` was reviewed at commit
`1864c7d`. It is a useful architectural reference, not a production design to
copy: it uses ESP32-S3, an SX1276-class raw-LoRa radio, TP4056X solar charging,
and a two-layer Eagle board with unresolved CAD/firmware inconsistencies.

Retained ideas:

- one hardware enable removes power from both the 5 V probe and RS485 domain;
- RS485 direction is automatic, so the MCU uses only UART TX/RX;
- external pull resistors make every power gate default OFF during reset.

Not retained:

- its ESP32-S3 GPIO map, raw-LoRa firmware, TP4056X charger, RF routing and
  battery connector;
- its unverified battery-ADC code path and unspecified protection parts;
- its PCB as a manufacturing or controlled-impedance reference.

Reference: [Makerfabs repository at the audited commit](https://github.com/Makerfabs/Industrial-grade-Soil-Remote-Monitor-V2/commit/1864c7ddf232020dcf8576e6238507a60789d60e).

## 4. Recommended system architecture

### 4.1 Power and signal block diagram

```text
6 V nominal solar panel
  -> keyed connector
  -> reverse-polarity / transient input protection
  -> LTC4079 VIN with Vmp-programmed input regulation

removable protected 1S 18650 cell in PCB holder + cell-contact NTC
  <-> LTC4079 BAT and NTC network

protected battery node
  -> reverse-polarity protection
  -> mechanical SYSTEM switch
  -> SYSTEM_RAW
       |-> TPS63900 buck-boost -> +3V3_SYSTEM
       |      |-> ESP32-C6-MINI-1-H4
       |      |-> DX-LR30-900M22S
       |      `-> TPS22917 load switch -> +3V3_RS485_SW -> THVD1406
       |
       `-> TPS61023 true-disconnect boost -> +5V_SOIL_SW
              `-> current protection -> external soil probe

protected cell voltage
  -> TPS22917 high-side switch
  -> precision divider + RC filter
  -> ESP32-C6 ADC

ESP32-C6
  <-> SPI + DIO/BUSY/RESET/RX/TX controls <-> DX-LR30 <-> 50-ohm trace <-> SMA
  <-> UART TX/RX <-> THVD1406 AutoDirection <-> SM712/protection <-> RS485 A/B
```

### 4.2 Mechanical switch behavior

`REQ`: the SYSTEM switch shall disconnect both the 3.3 V and 5 V system loads.
The panel, charger, removable cell and NTC remain connected so the cell can be
charged while the node is seasonally switched OFF.

No programming cable, RS485 line, GPIO header or antenna protection circuit
may back-power `+3V3_SYSTEM`, `+5V_SOIL_SW` or the cell. USB VBUS shall not be
connected to the service header or system power rail; the adapter uses 3.3 V
logic levels and reads the board's `3V3_REF` only.

The OFF-state storage current of the charger, cell protector and PCB leakage
shall be measured because the mechanical switch does not disconnect those
circuits from the cell.

## 5. MCU selection

### 5.1 Proposed orderable family

`PROPOSED`: `ESP32-C6-MINI-1-H4`.

Selection basis:

- 32-bit single-core RISC-V CPU up to 160 MHz;
- 4 MB in-package flash, sufficient for the sensor, Modbus, LoRaWAN, recovery
  and configuration firmware;
- 22 module GPIOs, which are required by the DX-LR30's separate RF-switch
  controls and the service interfaces;
- ROM UART0 programming and Serial monitor on dedicated GPIO16/GPIO17 pads;
- RTC/LP memory and timer deep sleep;
- module integration reduces crystal, flash, RF matching and assembly risk;
- `H4` temperature grade is specified to 105 deg C, subject to availability;
- ESP32-C6 deep-sleep current is specified as 7 uA typical with RTC timer and
  LP memory powered.

Wi-Fi, Bluetooth and 802.15.4 shall be disabled in normal firmware. Their
presence in the SoC is not a requirement to add any external parts.

ESP32-C3 reduces the headline timer-deep-sleep current from about 7 uA to about
5 uA, but its MINI module does not provide enough comfortable GPIO margin for
the nine DX-LR30 signals, independent programming UART0, RS485, sensor-domain
enable and switched battery measurement. ESP32-H2 and ESP32-S3 do not provide
a meaningful sleep-current advantage for this board and would create a new
firmware/PCB target. ESP32-C6 remains the best balance of pin margin, software
support, module availability and a comfortably sub-25-uA sleep budget.

### 5.2 ESP32 implementation requirements

- Follow the current Espressif module land pattern, antenna keep-out, power,
  CHIP_PU, reset and decoupling guidance.
- Put the module PCB antenna at the board edge and keep copper, enclosure metal,
  batteries and cables out of its keep-out even though the 2.4 GHz radios are
  disabled in deployed firmware.
- Do not fit an on-board USB-to-UART bridge or a USB connector in the baseline.
- Reserve GPIO16/GPIO17 exclusively for UART0 programming, boot log and Serial
  monitor. Do not share them with RS485 or another powered peripheral.
- Provide BOOT and reset/recovery access through buttons or production pads.
- Fit 10 kOhm pull-ups on GPIO8 and GPIO9. BOOT pulls GPIO9 to GND; do not add
  a large capacitor to GPIO9.
- Implement CHIP_PU with the current Espressif recommendation (starting point
  10 kOhm and 1 uF) plus a voltage supervisor/reset monitor chosen for the slow
  and interrupted ramp possible with solar charging.
- Do not use GPIO10/GPIO11; they are not exposed as ordinary module GPIOs.
- Treat GPIO4, GPIO5, GPIO8, GPIO9 and GPIO15 as strapping pins. Any use must be
  reviewed against their reset state and must not prevent normal boot.
- Add a 100 nF ADC input filter and perform ESP-IDF/Arduino ADC calibration.
- Provide 47-100 kOhm hardware pull-ups on LoRa NSS/RESET_N, pull-downs on
  RXEN/TXEN, and about 100 kOhm pull-downs on SENSOR_DOMAIN_EN and VBAT_DIV_EN.
- Confirm deep-sleep GPIO hold behavior for every enable and RF-control net.
  Firmware hold is additional protection; it does not replace external pulls.

Official design references:

- [ESP32-C6-MINI-1/MINI-1U datasheet](https://documentation.espressif.com/esp32-c6-mini-1_mini-1u_datasheet_en.html)
- [ESP32-C6 hardware design checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c6/schematic-checklist.html)
- [ESP32-C6 UART boot-mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp32c6/advanced-topics/boot-mode-selection.html)

## 6. LoRa and LoRaWAN subsystem

### 6.1 Radio module

`REQ`: retain the supplied `DX-LR30-900M22S` SPI radio module based on SX1262.

The manufacturer's current manual states:

- 1.8-3.7 V operating range, 3.3 V typical;
- 850-930 MHz RF range;
- 100 mA typical transmit current;
- 10 mA typical receive current;
- 180 nA software-shutdown current;
- +22 dBm maximum module capability;
- external antenna output;
- module pins 6/7 are separate RX/TX antenna-switch controls;
- module pins 13-19 provide DIO1, BUSY, RESET and SPI;
- DIO2 and DIO3 are not used by the current prototype.

Source: [DX-LR30-900M22S manufacturer manual](https://en.szdx-smart.com/static/upload/2025/05/08/202505081548.pdf).

The exact purchased module marking and mechanical revision shall be compared
with this manual before the footprint is released. The working prototype's
module pin mapping shall be treated as a reference, not copied blindly.

### 6.2 RF implementation

- Use one edge-mounted SMA female connector for LoRa only.
- Route module ANT to SMA as a controlled 50-ohm transmission line using the
  actual PCB fabricator stack-up.
- Keep the RF path short and on one layer; avoid vias where practical.
- Add a DNP pi-matching footprint if recommended by the RF designer.
- Select a low-capacitance RF ESD device only after its insertion loss and
  clamping performance are checked for the final frequency.
- Put the LoRa module and SMA near the board edge and away from both switching
  converter hot loops.
- Provide local 100 nF and 1 uF decoupling plus a bulk capacitor sized from TX
  transient measurement. A 47-100 uF footprint is recommended for bring-up.
- Fit default resistors that keep NSS inactive, RESET released and both RX/TX
  switch controls inactive during reset and deep sleep.
- Firmware shall enter radio software shutdown before ESP32 deep sleep.

The module's +22 dBm capability is not the default regulatory setting. The
LoRaWAN region, sub-band, channel plan, antenna gain and allowed conducted
power are `TBD` and shall be configured before field deployment.

## 7. Battery and solar subsystem

### 7.1 Replaceable cell and holder baseline

`REQ`: one user-replaceable 1S cylindrical 18650 Li-ion cell, nominal
3.6/3.7 V and 4.2 V charge termination, installed in a through-hole
PCB-mounted holder. The holder is part of the PCBA; the cell is not.

`PROPOSED` holder baseline: MPD `BK-18650-PC2`, because it is dimensioned for
the extra length of a protected 18650 cell. Keystone `1043P` with a retaining
cover is an alternative for mechanical review. Final selection must include:

- manufacturer-defined capacity;
- a protected cell or dedicated board protection circuit;
- mechanical retention suitable for transport, vibration and field service;
- clear polarity marking and reverse-insertion protection;
- cell-contact NTC clamped against the 18650 sidewall, not merely measuring PCB
  air temperature;
- a documented charge/discharge/storage temperature range;
- no user-selectable LiFePO4 mode.

The exact cell manufacturer, model, capacity, physical length, protection
thresholds, NTC curve and field replacement policy are `TBD`. A LiPo pouch is
not a drop-in alternative and would require a separate mechanical and safety
review. A bare unprotected 18650 shall not be accepted unless the approved PCB
contains the complete protection circuit.

Holder references: [MPD BK-18650-PC2](https://products.memoryprotectiondevices.com/?page_id=758)
and [Keystone 18650 holders](https://www.keystone-europe.com/wp-content/uploads/2019/11/battery-clips-contacts-holders.pdf).

If the selected cell is not protected, the PCB shall implement an
ultra-low-quiescent-current single-cell protector with back-to-back FETs. The
exact protection IC suffix and thresholds must be selected from the approved
cell datasheet. The charger shall not be treated as a substitute for cell
over-voltage, under-voltage, over-current and short-circuit protection.

### 7.2 Proposed charger

`PROPOSED` for Rev A: Analog Devices `LTC4079` linear 1-cell charger.

Useful features for this design include:

- 2.7-60 V input and 10-250 mA resistor-programmed charge current;
- CC/CV charge with C/10 or timer termination and automatic recharge;
- resistor-programmed input-voltage regulation that can hold a small panel near
  its measured Vmp without periodic Voc sampling;
- battery NTC qualification that pauses charging outside the selected window;
- reverse-current protection without a series Schottky diode;
- battery drain with VIN absent/open of 0.05 uA typical and 0.2 uA maximum.

It has no separate SYS power path. The protected battery node feeds the SYSTEM
switch directly. This is acceptable for this duty-cycled node, but charger
termination must be validated while the system is operating; timer termination
is preferred if simultaneous load current prevents reliable C/10 detection.

Source: [LTC4079 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ltc4079.pdf).

Release conditions:

- approve panel Voc, Vmp, Isc and rated power at the coldest expected panel
  temperature;
- set the input-regulation divider from measured panel Vmp;
- set charge current to the minimum of panel capability, cell limit and thermal
  limit, never above 250 mA;
- calculate worst-case linear-charger dissipation;
- validate charge startup with weak morning/evening illumination;
- prove charging is inhibited outside the approved cell temperature window;
- verify behavior with a missing, shorted or disconnected NTC;
- verify that SYSTEM OFF still allows charging and cannot power the load.

The soil HRS's 5-55 deg C charge window is a preliminary maximum window. The
selected cell may require narrower limits, especially for cold charging.

Charger alternatives reviewed:

| Part | Advantage | Why it is not the Rev A baseline |
|---|---|---|
| LTC4121-4.2 | synchronous buck with fractional-Voc MPPT, 50-400 mA | about 8.1 uA typical and up to roughly 18 uA battery-side dark current before the rest of the board; little margin to a 25 uA board limit |
| BQ25185 | robust SYS power path, NTC and 5 mA-1 A charge | battery-tracking VINDPM is not MPPT; 4 uA typical/5 uA maximum battery-only current |
| SPV1050 | buck-boost fractional-Voc harvester, 0.15-18 V after start | no complete external NTC-qualified conventional Li-ion charge profile; validation burden is too high for Rev A |
| BQ25504/BQ25570 | very low-IQ dynamic fractional-Voc harvesting | nominal 6 V panel can exceed their input absolute maximum; no external cell NTC |

If real winter testing shows that the LTC4079's linear loss is unacceptable,
`LTC4121-4.2` is the preferred efficiency-focused spin, with a relaxed sleep
current requirement.

### 7.3 Solar input protection

The solar connector and protection are `TBD` until the panel and cable are
selected. The released design shall include:

- keyed polarity;
- reverse-polarity protection with acceptable low-light voltage loss;
- input transient clamp selected above cold-panel Voc and below charger limits;
- input capacitor and filtering per charger guidance;
- cable strain relief and enclosure sealing;
- test points for panel voltage and charge current.

## 8. Power converters and domains

### 8.1 3.3 V system rail

`PROPOSED`: TI `TPS63900` buck-boost set to 3.3 V.

Reasons:

- accepts the cell/SYS voltage above and below 3.3 V;
- 75 nA quiescent current specified as typical and 1 uA maximum;
- 400 mA maximum output capability;
- input-current limiting and dynamic-voltage features;
- specified operating range covers the selected protected-cell voltage.

Source: [TPS63900 datasheet](https://www.ti.com/lit/ds/symlink/tps63900.pdf).

Before approval, verify the complete ESP32-C6 plus DX-LR30 transient load at
minimum cell voltage and maximum temperature. The design shall include local
bulk and high-frequency bypass capacitance and a brownout test with radio TX.

### 8.2 Switched 5 V sensor rail

`PROPOSED`: TI `TPS61023` synchronous boost set to 5.0 V.

The device provides a true load disconnect in shutdown, 0.1 uA shutdown current
specified as typical at 25 deg C, a 0.5-5.5 V input range and a 3.7 A typical
switch current limit. These are suitable features, but they do not prove that
the unknown soil probe load is supported.

Source: [TPS61023 datasheet](https://www.ti.com/lit/ds/symlink/tps61023.pdf).

Requirements:

- hardware pull-down keeps EN low during reset, boot and deep sleep;
- enable comes from one `SENSOR_DOMAIN_EN` GPIO;
- output capacitor and inductor are sized from measured startup/steady current;
- add a resettable fuse or controlled current-limit stage at the field connector;
- provide a discharge path so the 5 V rail does not remain charged after OFF;
- measure residual output voltage and input current in shutdown;
- prevent a powered sensor cable from feeding any internal rail.

### 8.3 Switched RS485 logic rail

`PROPOSED`: one `TPS22917` low-leakage load switch supplies `+3V3_RS485_SW` from
`+3V3_SYSTEM`. It is controlled by the same `SENSOR_DOMAIN_EN` signal as the 5 V
boost, with sequencing handled in firmware.

This keeps the transceiver, bias network and logic interface off during deep
sleep and avoids the 1-3 mA active/receive current of a permanently enabled
transceiver. The switch provides 1-5.5 V operation, 0.5 uA enabled quiescent
current typical, 10 nA disabled current typical and adjustable inrush control.

Source: [TPS22917 datasheet](https://www.ti.com/lit/ds/symlink/tps22917.pdf).

## 9. Battery voltage measurement

`REQ`: measure the protected cell node directly through the switched divider.
Do not measure the panel input or a converter output as a substitute for cell
voltage.

`PROPOSED` starting circuit:

```text
PROTECTED_BAT
  -> TPS22917 high-side switch
  -> 499k + 499k top resistance
  -> ADC node
  -> 249k bottom resistance
  -> GND

ADC node -> 100 nF -> GND
```

The nominal ratio is approximately 5.0:1. A 4.25 V cell produces approximately
0.85 V at the ADC, allowing use of the low ADC attenuation range. The divider
draws approximately 3.4 uA only while enabled. These are starting values, not
released values.

Firmware sequence:

1. drive `VBAT_DIV_EN` active;
2. wait at least the calculated five-time-constant settling period;
3. discard the first ADC conversion;
4. average calibrated samples;
5. drive `VBAT_DIV_EN` inactive;
6. report voltage and threshold state.

The released design shall include a tolerance, leakage, ADC input-impedance,
temperature and fault analysis. Calibration shall use at least two traceable
battery voltages. Firmware shall report voltage plus `normal`, `low` and
`critical`; it shall not report an accurate SOC percentage from voltage alone.
Thresholds and hysteresis are `TBD` from the selected cell and load behavior.

## 10. RS485 soil-probe interface

### 10.1 Proposed transceiver

`PROPOSED`: TI `THVD1406`, powered at 3.3 V from `+3V3_RS485_SW`.

Reasons:

- 3-5.5 V supply and 3.3 V MCU-compatible logic;
- integrated AutoDirection: no MCU DE or /RE control line;
- half-duplex operation to 500 kbps;
- extended +/-15 V operational common-mode range;
- open, short and idle-bus failsafe;
- high integrated IEC ESD ratings;
- 3 uA standby current typical, although the baseline power-gates it.

Sources: [THVD1406 product page](https://www.ti.com/product/THVD1406) and
[TI AutoDirection design note](https://www.ti.com/lit/pdf/slla574).

`RS485_DIR`, `DE` and `/RE` GPIOs are intentionally absent. Firmware sends and
receives only through UART TX/RX. The automatic driver's release behavior,
idle-bus bias, maximum cable and the exact probe baud rate shall be tested
together before release. In particular, THVD1406 releases a transmitted logic
HIGH after its internal timeout; therefore DNP fail-safe bias footprints are
mandatory and 4800/9600-baud operation must be proven on the real cable.

`MAX13487E/MAX13488E` is an alternate automatic-direction solution already
demonstrated by the Makerfabs topology. It requires 5 V supply and safe
level-shifting/clamping on the receiver output, and has higher active current;
use it only if THVD1406 low-baud validation fails.

### 10.2 Field protection and termination

- Fit an `SM712`-class asymmetric RS485 TVS at the connector.
- Fit two surge-rated series resistors, initially 10 ohms, between the TVS/
  connector and transceiver; validate the final pulse and signal-integrity
  requirements.
- Source: [Semtech SM712](https://www.semtech.com/products/circuit-protection/esd-protection/sm712).
- Provide DNP footprints for 120-ohm termination and fail-safe biasing.
- Power any bias network only from `+3V3_RS485_SW`.
- Do not fit termination until the cable length, sensor termination and baud
  rate are confirmed.
- Keep A/B high impedance when the sensor domain is off.
- Add a defined ground/reference conductor at the connector; an optional shield
  contact depends on the selected cable and enclosure grounding plan.
- Place TVS and surge-current return next to the connector with a short,
  low-inductance path.

### 10.3 Connector

`PROPOSED`: keyed 4-pin minimum connector:

| Pin | Signal |
|---:|---|
| 1 | +5V_SOIL_SW |
| 2 | GND/reference |
| 3 | RS485_A |
| 4 | RS485_B |

The exact connector family, pin order, cable length, wire colors and optional
shield/drain pin are `TBD`. The silkscreen shall label every signal and polarity.

## 11. ESP32-C6 pinout and electrical constraints

Pin-map revision: `C6-P1`, reviewed 2026-09-04. Module:
`ESP32-C6-MINI-1-H4`. All 22 exposed GPIOs are accounted for below.
The GPIO assignment from Draft 0.4 is retained; this revision adds module pad
numbers, explicit restrictions and off-domain interface requirements.

GPIO numbers are firmware identifiers, NOT physical package pad numbers.
The pad column refers to the MINI-1 module, not the bare ESP32-C6 chip or a
DevKit header. Firmware SPI/UART signals must be routed explicitly through the
GPIO matrix; do not rely on a generic Arduino board's default pins.

Types: A = analog input; I = input; O = output; OD = open-drain; - = reserved.
L/H are required circuit states, not a claim that the bare MCU guarantees them.
PU/PD are external pull-up/pull-down resistors. Values are design starting
points and require leakage/logic-margin review.

| GPIO | Module pad | Signal | Type | Required reset/boot condition | Timer deep sleep |
|---|---|---|---|---|---|
| 0 | 12 | VBAT_ADC | A | ADC1_CH0; divider OFF | analog; divider OFF |
| 1 | 13 | VBAT_DIV_EN | O | L / PD100k | L, held |
| 2 | 5 | SENSOR_DOMAIN_EN | O | L / PD100k | L, held |
| 3 | 6 | LORA_RXEN | O | L / PD100k | L, held |
| 4 | 9 | RESERVED_STRAP_MTMS | - | NC to loads; strap review | input buffer disabled |
| 5 | 10 | RESERVED_STRAP_MTDI | - | NC to loads; strap review | input buffer disabled |
| 6 | 15 | SERVICE_WAKE_N | I | H / PU100k; button to GND | wake input, H idle |
| 7 | 16 | LORA_TXEN | O | L / PD100k; no pad JTAG | L, held |
| 8 | 22 | RESERVED_BOOT_STRAP | - | H / PU10k | H; no field load |
| 9 | 23 | BOOT_N | I | H / PU10k; button to GND | H; no field load |
| 12 | 17 | RS485_TX | O | USB default; isolate off-domain | L or Hi-Z; USB disabled |
| 13 | 18 | RS485_RX | I | USB D+ default; isolate off-domain | input disabled; no back-power |
| 14 | 19 | LORA_RESET_N | O | H / PU47-100k | H, held |
| 15 | 20 | RESERVED_JTAG_STRAP | - | L / PD10k | L; no field load |
| 16 | 31 | UART0_TX | O | ROM log output; not a power gate | Hi-Z; adapter detached |
| 17 | 30 | UART0_RX | I | H / PU47-100k | input disabled; adapter detached |
| 18 | 24 | LORA_SCK | O | boot pull possible; NSS held H | L, held |
| 19 | 25 | LORA_MISO | I | radio data input; NSS held H | input buffer disabled |
| 20 | 26 | LORA_MOSI | O | boot pull possible; NSS held H | L, held |
| 21 | 27 | LORA_NSS | O | H / PU47-100k | H, held |
| 22 | 28 | LORA_DIO1 | I | radio IRQ input; no strap role | input buffer disabled |
| 23 | 29 | LORA_BUSY | I | radio BUSY input; no strap role | input buffer disabled |

### 11.1 Power pads and unavailable pins

- Module pad 3: `+3V3_SYSTEM`; pad 8: EN/CHIP_PU with pull-up, RC and reset supervisor.
- Pads 1, 2, 11, 14 and 36-53: GND, including the exposed ground-pad area.
- Pads 4, 7, 21 and 32-35: NC, leave unconnected.
- GPIO10/GPIO11 are not exposed by this module. The internal flash interface
  and unexposed chip signals are not expansion pins.
- GPIO0/GPIO1 are used for battery functions, so no external 32.768 kHz crystal
  can be added to those pins without redesign.
- There are no unallocated general-purpose GPIOs in this minimal map.

### 11.2 Pins to reserve or use conditionally

1. GPIO4, GPIO5, GPIO8, GPIO9 and GPIO15 are strapping pins. GPIO4/5 are
   reserved SDIO-edge/JTAG straps; GPIO8 stays HIGH; GPIO15 stays LOW. GPIO9
   is BOOT only. UART download requires GPIO8=1 and GPIO9=0 at reset; normal
   flash boot requires GPIO9=1. Do not add sensor loads or LEDs to these straps.
2. GPIO6/GPIO7 also have pad-JTAG functions MTCK/MTDO. They are reused here
   because the C6 module has a tight GPIO budget. External pad JTAG on GPIO4-7
   is incompatible with this wiring. Keep the reviewed USB-JTAG/eFuse source
   policy and never enable pad JTAG while attached to this circuit. Do not
   burn eFuses merely to implement this pin map.
3. GPIO12/GPIO13 are USB D-/D+ by default. No USB connector is fitted. Explicitly
   disable the USB pin function/pull-ups and route UART1 before enabling the
   RS485 domain. Provide powered-off isolation/buffering with specified Ioff
   on BOTH UART paths: GPIO13 can be driven/pulled by the USB block at boot.
   Firmware alone cannot prevent a boot-time back-power path through an
   unpowered transceiver. A series resistor alone is not proof of isolation.
4. GPIO16/GPIO17 are dedicated UART0 programming/Serial pins. ROM logs on TX
   mean they must never control a power gate or valve.
5. GPIO18-23 have SDIO alternate functions and startup pulls. They are valid
   LoRa GPIO-matrix pins, not boot straps. Keep NSS HIGH, initialize SPI
   explicitly and disable unwanted pulls/input buffers before sleep.
6. GPIO0-7 are LP/RTC-capable; only GPIO6 is assigned external wake. BOOT is
   recovery, not the baseline wake button.

### 11.3 LoRa connection cross-reference

| DX-LR30 signal | Radio module pin from project reference | ESP32-C6 GPIO / module pad |
|---|---:|---|
| RXEN | 6 | 3 / 6 |
| TXEN | 7 | 7 / 16 |
| DIO1 | 13 | 22 / 28 |
| BUSY | 14 | 23 / 29 |
| RESET | 15 | 14 / 19 |
| MISO | 16 | 19 / 25 |
| MOSI | 17 | 20 / 26 |
| SCK | 18 | 18 / 24 |
| NSS | 19 | 21 / 27 |

Radio VCC is 3.3 V (project-reference pin 9), never a GPIO supply.
DIO2/DIO3 are not connected in the baseline. Confirm the exact purchased
DX-LR30 revision and footprint before schematic release.

### 11.4 Defined enable and sleep sequence

- GPIO2 HIGH enables TPS61023 EN and the RS485-rail load switch together;
  GPIO2 LOW disables both. UART isolation follows this power-domain enable.
- GPIO1 HIGH enables the high-side battery-divider switch; LOW disconnects
  the divider. The GPIO never carries battery or sensor load current.
- Before sleep: complete Modbus, disconnect UART paths, set GPIO2/1 LOW,
  command radio shutdown, set RXEN/TXEN LOW, NSS/RESET_N HIGH and SCK/MOSI LOW.
- Configure ESP32-C6-supported GPIO/LP hold before deep sleep and establish
  safe levels before releasing it after wake. External pulls remain mandatory.
- Remove the USB-to-TTL adapter for sleep-current acceptance. No continuously
  asserted input pull-up or fault line may be omitted from the leakage budget.
- Charger-status input and user LED are intentionally not allocated; adding
  either requires a pin-map revision, not silent sharing.

### 11.5 Verification sources

Pad/strap facts were checked against Espressif's
[C6-MINI module datasheet v1.5, Sections 3-4](https://documentation.espressif.com/esp32-c6-mini-1_mini-1u_datasheet_en.pdf).
USB defaults and GPIO startup behavior follow the
[C6 hardware checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c6/schematic-checklist.html).
The assignment itself is a project engineering proposal, not an Espressif
reference design. Schematic ERC, startup waveforms and assembled sleep current
are still release gates.

## 12. User interface and service access

Proposed keyed connector order (same order on the universal board):

| J_PROG pin | Board signal | Adapter connection |
|---:|---|---|
| 1 | GND | GND |
| 2 | 3V3_REF | High-impedance reference input only; otherwise leave open |
| 3 | UART0_TX / GPIO16 | adapter RX |
| 4 | UART0_RX / GPIO17 | adapter TX |
| 5 | DTR | through onboard auto-reset circuit |
| 6 | RTS | through onboard auto-reset circuit |

This is not a universal adapter pin standard. Adapter 3.3 V/5 V supply outputs
must remain disconnected; the board powers itself. EN and BOOT have separate
buttons/pads.

- One optional charge-status LED driven by the charger, DNP by default or with
  a cut jumper.
- No GPIO user/status LED in the baseline BOM.
- No always-on power LED.
- One service/wake button and BOOT/reset access, subject to final GPIO review.
- A keyed 1x6 header or Tag-Connect footprint for external 3.3 V USB-to-TTL:
  GND, 3V3_REF, UART0_TX, UART0_RX, DTR and RTS.
- `3V3_REF` is a logic-level reference/output, not a power input. The external
  adapter shall not power the board by default; program with the battery fitted
  and SYSTEM ON. Never connect a 5 V TTL signal to an ESP32 GPIO.
- UART0_TX from the board connects to adapter RX; UART0_RX connects to adapter
  TX; grounds must be common.
- Route DTR and RTS through the standard Espressif two-transistor automatic
  boot/reset circuit to GPIO9/BOOT and EN/CHIP_PU. DTR/RTS are active LOW and
  shall not be connected directly to ESP32 pins. Fit the associated EN
  capacitor required by the Espressif reference circuit.
- The auto-reset components may have a DNP build option, but their footprints
  remain in the baseline PCB. Provide BOOT and RESET buttons or accessible pads
  regardless, so a basic TX/RX/GND adapter can always program the board.
- Opening a serial port may toggle DTR/RTS. Verify that the selected terminal
  and auto-reset network cannot hold the node in reset or create a reset loop.
- Production pads for 3V3, GND, EN/reset, BOOT, UART0 and all power enables.
- A removable zero-ohm link or jumper for complete-board current measurement.
- DNP expansion pads may expose only electrically safe spare pins, 3V3 and GND;
  field wiring shall not be attached to strapping or RF-critical signals.

## 13. Low-power and energy requirements

### 13.1 Hardware states

| State | Charger | 3.3 V | DX-LR30 | RS485 rail | 5 V probe | ESP32 |
|---|---|---|---|---|---|---|
| SYSTEM OFF | connected | off | off | off | off | unpowered |
| Deep sleep | connected | on | software shutdown | off | off | timer deep sleep |
| Measurement | connected | on | standby | on | on | active |
| TX/RX1/RX2 | connected | on | active | off | off | active |
| Service | connected | on | as commanded | as commanded | as commanded | active |

### 13.2 Sleep-current target

`REQ`: complete assembled-board current shall be <=25 uA in normal timer deep
sleep. This is compatible with ESP32-C6: the module specifies 7 uA typical with
RTC timer and LP memory powered, leaving useful margin for the charger,
regulator, radio shutdown, cell protection and leakage.

`REQ`: every non-operating firmware state shall remain below 1 mA. This includes
normal deep sleep, retry backoff after sensor/LoRa failure, unjoined-network
backoff and recovery from a brownout. Hardware pull resistors must keep the
sensor rail, RS485 rail and battery divider OFF even before firmware starts.
Firmware shall never remain indefinitely awake while retrying a failed sensor
or LoRaWAN join.

The <1 mA limit cannot apply while the node intentionally boots, measures the
probe, transmits/receives LoRaWAN, is programmed over UART, or charges the
battery. ESP32 active current, the approximately 100 mA DX-LR30 TX current and
the unknown 5 V probe current necessarily exceed 1 mA in those bounded states.

Indicative typical-current contributors from current datasheets are:

| Contributor | Indicative typical value | Release note |
|---|---:|---|
| ESP32-C6 timer deep sleep + LP memory | 7 uA | Typical, not a guaranteed board maximum |
| DX-LR30 software shutdown | 0.18 uA | Exact purchased revision must be measured |
| LTC4079, VIN absent/open | 0.05 uA | 0.2 uA maximum from battery |
| TPS63900 enabled, no load/not switching | 0.075 uA | 1 uA maximum specified |
| TPS61023 shutdown | 0.1 uA | 25 deg C condition |
| Two TPS22917 switches disabled | approximately 0.02 uA typical total | Temperature maximum must be budgeted |
| Ultra-low-IQ cell protector | approximately 0.6 uA typical | Exact circuit or protected-cell PCM TBD |

The indicative sum is approximately 8.0 uA before PCB leakage, pull networks,
protection devices and tolerances. A commercial protected cell may contain a
higher-IQ PCM, so the selected cell must be measured. The roughly 17 uA margin
to the 25 uA target is sufficient for the proposed architecture but is not a
substitute for an assembled-board test. The schematic review shall contain a
maximum-leakage budget over the approved temperature range.

Deep-sleep acceptance setup:

- SYSTEM switch ON;
- solar input disconnected;
- laboratory battery source at 3.70 V unless the test plan specifies a sweep;
- no USB-to-TTL/programmer attached;
- soil probe disconnected and both switched domains confirmed OFF;
- DX-LR30 commanded to software shutdown;
- LEDs disabled/off;
- measurement made after current has stabilized;
- repeat at minimum and maximum approved battery voltage and temperature.

Before entering normal deep sleep, firmware shall put DX-LR30 into shutdown,
force SENSOR_DOMAIN_EN and VBAT_DIV_EN low, force RXEN/TXEN low, hold NSS and
RESET_N high, remove the RS485 rail, place the GPIO12 RS485 TX signal in a
non-back-powering state, then apply GPIO hold. Wake code shall establish safe
output levels before releasing hold.

### 13.3 Autonomy and panel sizing

Battery capacity and panel power shall be selected from measured charge per
cycle, not component headline currents.

The design report shall calculate:

```text
Q_day = N_cycles * Q_active_cycle + I_sleep * t_sleep_total

C_required = Q_day * autonomy_days
             / (allowed_depth_of_discharge
                * cold_capacity_factor
                * aging_factor
                * power_conversion_factor)

P_panel_required = E_day
                   / (winter_peak_sun_hours
                      * charge_path_efficiency
                      * weather_margin)
```

`TBD`: measurement/reporting interval, minimum winter autonomy, installation
location, winter solar resource, battery aging margin and allowable low-battery
behavior.

## 14. PCB, EMC and environmental requirements

### 14.1 Stack-up

`PROPOSED` cost baseline: two-layer FR-4 PCB.

- Put most components and critical routing on top.
- Keep the opposite layer as close as practical to a continuous ground plane;
  do not route signals through the RF return path or converter hot loops.
- Use ground stitching around board edges, the RF route and noisy power zones.
- Route module ANT to SMA as a fabricator-calculated 50-ohm coplanar waveguide
  with ground (CPWG), using the released board thickness, copper and soldermask.

Four layers are not intrinsically required by the function. They become the
preferred option only if the board must be very compact, the two-layer ground
return is excessively cut, EMC testing fails, or the fabricator cannot produce
a practical 50-ohm CPWG. A four-layer alternative uses L2 as an uninterrupted
ground plane and makes RF impedance and converter return paths easier, at
higher PCB cost.

"Controlled 50 ohm" does not mean placing a 50-ohm resistor in series with the
antenna. It means that the ANT-to-SMA copper geometry has a 50-ohm
characteristic impedance. The fabricator must calculate/confirm trace width,
gap to ground, dielectric thickness and impedance tolerance for the actual
stack-up. Keep this trace short, avoid vias and stubs, maintain continuous
ground under/alongside it, and use a via fence.

### 14.2 Layout rules

- Follow TI reference layouts for every switching converter; minimize each hot
  current loop.
- Keep switching nodes and inductors away from the LoRa module, SMA path, ADC
  divider and antenna regions.
- Keep an uninterrupted reference plane under SPI, UART and RS485 logic traces.
- Enforce ESP32 module antenna keep-out and LoRa antenna mechanical clearance.
- Put connector protection at the connector, before traces enter the board.
- Separate surge-current returns from ADC and RF grounds by placement and path,
  without splitting the RF reference plane incorrectly.
- Use keyed, locking connectors and clear polarity/signal silkscreen.
- Add test points without creating RF stubs or leakage paths.
- Use conformal-coating keep-out markings around connectors, switch contacts,
  test pads and the RF connector.
- Place the DX-LR30 according to the manufacturer land pattern and reflow
  guidance; confirm module MSL and assembly-side sequence.

### 14.3 Environment and mechanics

`TBD`: PCB outline, mounting holes, enclosure, IP target, antenna placement,
cable glands, condensation venting, coating material, maximum cable length and
field service method.

The final approved ambient range cannot exceed the battery, DX-LR30, connector,
switch or enclosure limits even though the proposed ESP32 module is 105 deg C
grade. Battery charge and discharge ranges shall be stated separately.

## 15. Firmware, provisioning and diagnostics requirements

The production-oriented firmware shall:

- use LoRaWAN Class A, not raw LoRa, for normal operation;
- keep Wi-Fi/BLE/802.15.4 disabled;
- assert all hardware enables to safe OFF before peripheral initialization;
- power the sensor and transceiver only for a bounded measurement window;
- implement Modbus timeout, CRC, exception and invalid-value error categories;
- use bounded retries and always remove sensor power after failure;
- store the configurable interval and LoRaWAN session state safely across deep
  sleep;
- deduplicate every downlink command by a command ID, including `sample now`;
- send an immediate application acknowledgement/result when a command executes;
- use a versioned payload with explicit hardware, sensor and battery status;
- handle low and critical battery states without repeated brownout boot loops;
- put the radio in shutdown and verify both sensor domains OFF before sleep;
- exclude fixed delays and always-on serial logging from the production path;
- contain no deployed OTAA secrets in tracked source files;
- support unique fixture-provisioned DevEUI, JoinEUI and AppKey;
- define secure-boot and flash-encryption policy before production release;
- keep source code and code comments in English.

The bring-up firmware shall separately test:

- charger/status indication and battery ADC;
- 3.3 V rail load/transient response;
- 5 V rail enable, load, short and discharge behavior;
- RS485 transmit/receive direction and Modbus frames;
- DX-LR30 SPI, RF switch pins, reset, TX/RX and shutdown;
- LoRaWAN join, uplink, RX1/RX2 and command acknowledgement;
- timer wake, repeated deep sleep and brownout recovery;
- all buttons, LEDs and production test pads.

## 16. Contractor deliverables

The PCB contractor shall provide:

1. native KiCad, Altium or approved EDA project including all libraries;
2. schematic PDF and design-rule/ERC report;
3. approved stack-up and controlled-impedance calculation;
4. complete BOM with exact manufacturer part numbers, approved alternates,
   lifecycle status and supplier references;
5. Gerber, NC drill, board drawing, IPC netlist and fabrication notes;
6. centroid/pick-and-place, assembly drawings and stencil notes;
7. complete populated PCBA STEP model and enclosure fit check;
8. power-tree, charger thermal, battery/solar autonomy and sleep-current
   calculation reports;
9. RF layout review and antenna/region assumptions;
10. bring-up firmware source and reproducible build instructions;
11. production programming/provisioning procedure without shared OTAA keys;
12. test fixture definition and manufacturing test procedure;
13. prototype bring-up, environmental and current measurement report;
14. design source archive with revision, hash and release notes.

## 17. Verification and acceptance matrix

| Test | Draft acceptance condition |
|---|---|
| Visual/assembly | Correct polarity, no assembly defects, module/connector alignment verified |
| 3.3 V rail | 3.3 V within the approved tolerance at sleep, boot and radio TX load |
| 5 V rail ON | 5.0 V within approved tolerance at measured sensor startup and steady load |
| 5 V rail OFF | Output discharged; no sensor back-power; shutdown current inside budget |
| RS485 | Repeated valid Modbus transactions over maximum approved cable; timeout/CRC/exception tests pass |
| LoRa | SPI and RF controls pass; join/uplink/RX1/RX2 work in the approved regional plan |
| Battery ADC | Meets the approved error after two-point calibration over voltage and temperature |
| Charge | Correct current/termination from the selected panel; thermal limits pass |
| NTC inhibit | Charging disabled outside selected cell limits and on required NTC faults |
| SYSTEM OFF | MCU, radio, RS485 and 5 V probe unpowered; charger remains functional |
| Timer deep sleep | Complete assembled board <=25 uA in the Section 13.2 setup |
| Inactive/error backoff | <1 mA; no indefinite active polling/join/sensor retry state |
| Brownout | No unsafe rail enable, corrupted configuration or uncontrolled reboot loop |
| RF/EMC/ESD | Passes the approved product-level test plan; IC headline ratings alone are insufficient |
| Provisioning | Unique identity/keys and matching QR/serial label verified |
| Environmental | Passes approved temperature, condensation and enclosure tests |

Exact rail tolerances, cable length, ESD/EFT/surge levels, sample count and
temperature points are `TBD` in the formal design verification plan.

## 18. Approval gate before schematic capture

The following baseline recommendations should be explicitly accepted or
changed by the product owner:

1. `ESP32-C6-MINI-1-H4` with 4 MB flash;
2. one removable protected 1S 18650 cell in a PCB holder with sidewall-contact NTC;
3. LTC4079 fixed-Vmp solar charger, conditional on panel, energy and thermal validation;
4. TPS63900 3.3 V and TPS61023 switched 5 V converters;
5. DX-LR30-900M22S with one external LoRa SMA and no GPS;
6. THVD1406 automatic-direction RS485 with switched 3.3 V supply and SM712,
   conditional on real 4800/9600-baud bus validation;
7. no external RTC, ADC or fuel gauge;
8. two-layer PCB with calculated 50-ohm CPWG; move to four layers only if review/test requires it;
9. complete-board timer deep sleep <=25 uA and every inactive/error-backoff
   state <1 mA; intentional boot, sensing, radio and charging are excluded;
10. no on-board USB interface; external 3.3 V USB-to-TTL on dedicated UART0
    GPIO16/GPIO17, DTR/RTS through the standard two-transistor auto-reset
    circuit, and independent BOOT/RESET access;
11. provisional GPIO map in Section 11.

## 19. Blocking inputs before schematic release

1. Exact protected 18650 cell, capacity, physical length, protection PCM,
   selected holder/retainer and sidewall-contact NTC.
2. Solar panel Voc, Vmp, Isc, rated power, cable and connector.
3. Exact soil-probe model, supply range, startup/steady/peak current, startup
   time, Modbus address/framing/registers and cable length.
4. Exact DX-LR30 label/revision, footprint drawing and antenna connector choice.
5. Deployed LoRaWAN regional plan, allowed power and antenna gain.
6. Board outline, mounting holes, enclosure, switch, cable glands and IP target.
7. Required ESD, EFT and surge test levels for solar and sensor cables.
8. Measurement interval, minimum winter autonomy and critical-battery policy.
9. Required sampling-time accuracy over seasonal temperature.
10. Production quantity, target cost, preferred suppliers and allowed packages.
11. Service/programming connector and whether a user wake button is required.
12. Secure boot, flash encryption and credential provisioning policy.

Gerbers and production BOM shall not be released until these inputs, the
schematic, the maximum-leakage budget and the energy budget are approved.
