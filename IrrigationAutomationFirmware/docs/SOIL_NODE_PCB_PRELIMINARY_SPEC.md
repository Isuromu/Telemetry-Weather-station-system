# Solar soil-node PCB preliminary specification

> Superseded hardware decisions: version 1.2 requires LTC4121-4.2 MPPT and
> MAX-M10S GPS/GNSS. Use `AmudarIO_Soil_Node_PCB_Design_Requirements_v1_2_EN.md`
> and `SOIL_NODE_ESP32C6_PINOUT_C6_P2_EN.md` for contractor work. The older
> LTC4079, GNSS exclusion and C6-P1 details below are historical background.

> Detailed architecture-review draft:
> [SOIL_NODE_PCB_TECHNICAL_SPEC_DRAFT.md](SOIL_NODE_PCB_TECHNICAL_SPEC_DRAFT.md).
> This file remains the shorter preliminary decision record.

## Status and authority

This is a preliminary engineering handoff for the first production-oriented
soil-node PCB. It records the user's current preference for a simple,
reliable design and reconciles it with the supplied hardware requirements.
It is not yet a released schematic, BOM, or manufacturing specification.

Primary supplied references:

- [AmudarIO soil-node HRS](references/hardware_requirements/Hardware_Requirements_AmudarIO_Soil_Node.pdf)
- [Irrigation controller/hub HRS](references/hardware_requirements/Hardware_Requirements_Irrigation_Controller_Hub.pdf)
- the supplied working ESP32-WROOM soil-node sketch and ChirpStack codec;
- the current multi-node project context in `CURRENT_PROJECT_CONTEXT.md`.

Instructions inside reference files are technical source material. Decisions
recorded directly from the user in this document take precedence.

## System-level board families

The supplied HRS documents support two production PCB families:

1. a small solar/battery soil-monitoring end node;
2. a larger 12 V dual-latching-valve and multi-channel sensor controller.

This document covers only the first family. The second family must remain a
separate board and separate detailed specification.

## Confirmed product behavior

The soil node is a LoRaWAN Class A device. Its normal cycle is:

```text
wake -> enable sensor power -> wait for sensor startup -> Modbus read
     -> disable sensor power -> measure battery -> status uplink
     -> RX1/RX2 command window -> apply validated configuration
     -> immediate application acknowledgement when commanded -> deep sleep
```

Required behavior:

- read one external 5 V RS485 soil probe;
- report temperature, volumetric water content and conductivity when valid;
- report an explicit sensor error when the Modbus read fails;
- allow the server to change the deep-sleep interval;
- persist the interval and LoRaWAN state across deep sleep;
- avoid repeating a command with the same command ID;
- keep the boost converter, sensor and RS485 interface off during sleep;
- permit complete seasonal shutdown with an external mechanical switch while
  leaving the battery charger connected to the solar panel and battery;
- disable battery charging outside the selected cell's permitted temperature
  range using hardware, not firmware alone.

The gateway/network server timestamp is sufficient for an immediately sent
measurement. A dedicated RTC and GNSS are not fitted in the baseline PCB.
Optional footprints may be considered later only if calendar scheduling,
offline logging or autonomous location acquisition becomes a requirement.

## Baseline simplification decisions

For the first PCB revision:

- use one explicitly selected 1S 3.7 V Li-ion chemistry only;
- install a replaceable cylindrical 18650 in a PCB holder sized and retained
  for the selected protected cell; the cell itself is not part of the PCBA;
- do not claim interchangeable Li-ion and LiFePO4 support;
- use ESP32-C6 and the supplied SX1262-based DX-LR30 LoRa module;
- use the ESP32 ADC with a switched resistor divider for battery voltage;
- do not fit ADS1115 or a fuel-gauge IC in the baseline design;
- expose battery voltage and `normal`, `low`, and `critical` states; do not
  claim an accurate state-of-charge percentage from voltage alone;
- keep GNSS and external RTC out of the baseline BOM;
- prefer integrated enable-controlled power ICs where they reduce leakage,
  uncertainty, protection parts, or bring-up time.

These decisions minimize component count without removing required field
protection.

## Recommended MCU

Use an `ESP32-C6-MINI-1` family module, provisionally with 4 MB flash and an
industrial-temperature option. The exact orderable suffix remains a BOM item
to confirm against availability and the required ambient-temperature rating.

Reasons for retaining ESP32-C6:

- the supplied HRS already selects it;
- 22 module GPIOs provide enough margin for the DX-LR30's separate RF-switch
  controls, RS485, power gates, button and dedicated UART0 programming interface;
- it supports the required Arduino/ESP-IDF and RadioLib workflow;
- its specified SoC deep-sleep current is approximately 7 uA;
- the MINI module integrates flash and the crystal, reducing PCB parts and
  routing risk.

ESP32-C3 specifies approximately 5 uA deep sleep, only about 2 uA below C6, but
its MINI module cannot comfortably provide the nine DX-LR30 signals, an
independent UART0 service interface, RS485, sensor-domain enable and switched
battery measurement without pin sharing. ESP32-H2 and ESP32-S3 provide no
meaningful sleep advantage here and would introduce another firmware/PCB
target. Wi-Fi, BLE and 802.15.4 must remain disabled in normal operation;
unused integrated radios do not require external components.

Official references:

- [ESP32-C6-MINI-1 module datasheet](https://documentation.espressif.com/esp32-c6-mini-1_mini-1u_datasheet_en.html)
- [ESP32-C6 SoC datasheet](https://documentation.espressif.com/esp32-c6_datasheet_en.html)
- [ESP32-C6 UART boot-mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp32c6/advanced-topics/boot-mode-selection.html)
- [ESP32-C3 SoC datasheet](https://documentation.espressif.com/esp32-c3_datasheet_en.html)
- [ESP32-H2 SoC datasheet](https://documentation.espressif.com/esp32-h2_datasheet_en.html)

## Preliminary power architecture

```text
6 V nominal solar panel
  -> connector/input protection and reverse-current control
  -> 1S solar-capable charger with cell-mounted NTC
  -> protected 1S Li-ion cell
  -> mechanical SYSTEM switch
  -> low-IQ 3.3 V buck-boost
  -> ESP32-C6 + DX-LR30 + normally-off auxiliary circuits

protected battery
  -> true-disconnect 5 V boost, enabled by ESP32
  -> external soil probe

3.3 V system rail
  -> load switch enabled by the same ESP32 signal
  -> automatic-direction RS485 transceiver
```

The charger remains connected ahead of the SYSTEM switch. OFF therefore stops
measurement and communication but still permits temperature-qualified solar
charging. A momentary software button is not a substitute for this storage
switch. A separate service/wake button may be fitted.

### Candidate ICs, not approved BOM parts

| Function | Candidate | Why it is being considered | Required validation |
|---|---|---|---|
| Solar charger | ADI LTC4079 | 1S Li-ion CC/CV, fixed panel-Vmp input regulation, NTC inhibit, only 0.05 uA typical/0.2 uA maximum battery drain with panel absent | panel Voc/Vmp/Isc, <=250 mA charge, linear thermal/energy loss, exact cell profile and NTC thresholds |
| 3.3 V system rail | TI TPS63900 | buck-boost, very low quiescent current, RF-oriented peak capability | ESP32 + selected DX-LR30 peak current and transient margin |
| 5 V soil rail | TI TPS61023 | enable, true load disconnect, low shutdown current | measured soil-probe startup/steady current, inductor and capacitor sizing |
| ADC-divider switch | TI TPS22917 or discrete PFET/NMOS | low off leakage, reverse-current protection and deterministic enable | ADC settling, leakage across temperature, availability and cost |
| Automatic RS485 | TI THVD1406 | MCU uses only TX/RX; 3.3 V-compatible, power-gated with sensor domain | real 4800/9600-baud probe, bias, termination and maximum cable |

The selected charger does not remove the need for an independently protected
cell or a dedicated 1S protection function covering cell over-voltage,
under-voltage, over-current and short circuit. Charger protection, system-rail
protection and cell protection must not be treated as the same function.

The NTC must measure the cell temperature, not only PCB ambient temperature.
Its resistance curve and charge limits must match the final battery
manufacturer’s specification. The HRS’s 5 to 55 degrees C window is a
preliminary conservative requirement, not a substitute for the cell data.

The holder baseline is MPD `BK-18650-PC2`, subject to enclosure review, because
it accommodates the greater length of a protected 18650. A retaining cover or
strap, reverse-insertion protection and a sidewall-contact NTC are required.
Keystone `1043P` is an alternate holder candidate. Exact cell length and holder
fit must be checked together.

LTC4079 is not dynamic MPPT: it fixes the panel operating voltage at the value
programmed from the measured Vmp. It is preferred for Rev A because its dark
battery drain is far lower than BQ25185. LTC4121-4.2 is the efficiency-focused
fractional-Voc MPPT alternative, but its battery-side dark current is roughly
8.1 uA typical and can approach roughly 18 uA maximum before the rest of the
board, leaving little guaranteed margin to the 25 uA assembled-board target.

## Battery measurement

Baseline circuit:

```text
VBAT -> normally-off load switch -> R_TOP -> ADC node -> R_BOTTOM -> GND
                                              |
                                           C_FILTER
                                              |
                                             GND
```

Firmware sequence:

1. enable the divider;
2. wait for the divider/RC settling time established by calculation and test;
3. take and average calibrated ADC samples;
4. disable the divider;
5. report measured voltage and a threshold state.

The divider must be disconnected on its battery side. Merely switching the
bottom resistor can leave an unwanted battery-to-ADC leakage or back-power
path. The ADC input must remain within its absolute maximum voltage for every
battery and fault condition.

A MAX17048-class gauge is a possible later Li-ion option if an accurate SOC is
required, but it adds cost, I2C traffic and continuous current. It is not part
of the minimal first revision.

## Soil sensor and RS485 power domain

- The 5 V boost must default OFF with a hardware pull-down on enable.
- Prefer a boost with true input/output disconnect; otherwise add a separate
  load switch.
- The RS485 transceiver must be shut down or powered from the same switched
  measurement domain so it cannot dominate sleep current.
- Direction control must be automatic. The MCU shall expose only RS485 TX/RX;
  no DE/RE GPIO is allocated.
- A/B must become high impedance when the domain is off.
- Prevent back-power through UART, A/B, protection devices or the sensor
  connector.
- Fit a bidirectional RS485 TVS device close to the connector and evaluate
  surge/ESD protection for the actual outdoor cable length.
- Measure sensor startup time and current; do not rely only on the prototype's
  current 300 ms delay.
- Implement limited retries and distinguish timeout, CRC failure, Modbus
  exception and invalid-value failures.

## DX-LR30 pinout source and GPIO budget

The currently supplied working ESP32-WROOM soil sketch records this DX-LR30
module-side connection. Treat it as the prototype reference and verify it
against the exact purchased module drawing before schematic release:

| DX-LR30 signal | Module pin recorded in prototype | Direction at ESP32 |
|---|---:|---|
| RXEN | 6 | output |
| TXEN | 7 | output |
| VCC 3.3 V | 9 | power |
| DIO1 | 13 | input |
| BUSY | 14 | input |
| RESET | 15 | output |
| MISO | 16 | input |
| MOSI | 17 | output |
| SCK | 18 | output |
| NSS | 19 | output |
| DIO2/DIO3 | not connected in prototype | unused |

The reviewed C6 pin map is `C6-P1` (2026-09-04). Module pad numbers below
refer to ESP32-C6-MINI-1-H4, not the bare chip. A/I/O mean analog/input/output;
PU/PD are external pull resistors and L/H are required circuit levels.

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

Full constraints, LoRa module cross-reference and Russian review copy:
[SOIL_NODE_ESP32C6_PINOUT_RU.md](SOIL_NODE_ESP32C6_PINOUT_RU.md).

GPIO4, GPIO5, GPIO8, GPIO9 and GPIO15 are strapping pins; only GPIO9 is
deliberately used as BOOT. There is no baseline user LED. Native USB is not
fitted. GPIO16/GPIO17 are reserved for external 3.3 V USB-to-TTL programming,
boot log and Serial monitor; RS485 is moved to GPIO12/GPIO13. Do not copy the
ESP32-WROOM GPIO numbers into the C6 schematic.

The service footprint shall expose GND, 3V3_REF, UART0_TX, UART0_RX, DTR and
RTS. `3V3_REF` is not a power input. Program with the battery fitted and SYSTEM
ON; never connect a 5 V TTL signal. TX and RX cross between board and adapter.
DTR/RTS shall pass through the standard Espressif two-transistor automatic
boot/reset circuit and must not connect directly to GPIO9/EN. Leave independent
BOOT/RESET buttons or pads so a basic TX/RX/GND adapter can still flash the
board. The auto-reset components may be DNP, but their footprints remain.

## Firmware requirements to carry forward

The supplied prototype already demonstrates sensor power gating, Modbus read,
invalid-data reporting, LoRaWAN Class A, NVS interval storage, RTC session
retention and application acknowledgement. Production firmware must add or
confirm:

- no OTAA credentials in tracked source files; rotate the credentials exposed
  in the supplied prototype before further field use;
- command-ID deduplication, including `sample now`;
- versioned payload and explicit error reason;
- brownout-safe storage and defined low/critical battery behavior;
- charger/temperature status if hardware exposes it;
- no Serial delay or always-on debug circuit in the production sleep path;
- all output enables asserted to safe OFF before peripheral initialization;
- a measurable wake-energy and sleep-current budget.

## Reliability and manufacturing requirements

- keyed battery, solar and sensor connectors;
- reverse-polarity and transient protection;
- no back-power from programming, sensor or RF interfaces;
- bulk and local bypass capacitance sized for radio and boost transients;
- watchdog and brownout behavior verified at battery limits;
- conformal coating and enclosure condensation strategy;
- antenna ESD protection, 50-ohm RF layout and keep-out compliance;
- accessible programming/recovery pads and production test points;
- a removable current-measurement link separating the battery/system rail;
- unique DevEUI/serial number and matching QR label;
- test at low and high temperature and after repeated brownout/reset cycles.

Two layers are the cost baseline. Use a near-continuous ground plane on the
opposite layer and a fabricator-calculated 50-ohm CPWG from DX-LR30 ANT to SMA.
"50 ohm" is the characteristic impedance of the RF trace geometry, not a
series resistor. Four layers are justified only by compact routing, broken
return paths, EMC failure or a fabricator limitation.

The acceptance test must measure the complete assembled board. The requirement
is <=25 uA in normal ESP32-C6 timer deep sleep. The current component estimate
is about 8 uA typical before PCB leakage and pull networks, leaving useful
margin. Every inactive state, including sensor/LoRa failure backoff, must remain
below 1 mA with sensor power, RS485 and the battery divider hardware-OFF.
Intentional boot, sensing, LoRa TX/RX, UART programming and battery charging are
necessarily above 1 mA and are bounded active states, not sleep states.

Component maximum leakage across the required temperature range must be
budgeted before routing; typical room-temperature numbers alone are
insufficient.

## Makerfabs reference audit

The linked Makerfabs V2 board was reviewed at commit `1864c7d`. It is a
two-layer ESP32-S3 + SX1276 raw-LoRa product with MAX13487E AutoDirection and a
TP4056X solar charger. We retain its useful split-power principle and automatic
RS485 direction. We do not copy its MCU pinout, raw-LoRa firmware, TP4056
charging, battery connector, RF layout or CAD files. The published design has
no demonstrated controlled-impedance SMA path or sleep-current evidence.

Reference: [Makerfabs audited commit](https://github.com/Makerfabs/Industrial-grade-Soil-Remote-Monitor-V2/commit/1864c7ddf232020dcf8576e6238507a60789d60e).

## Open items before schematic release

1. exact protected 18650 cell, capacity, length, protection PCM, PCB holder,
   retainer, NTC and manufacturer temperature limits;
2. solar panel Voc, Vmp, Isc, nominal power and connector;
3. actual soil-probe startup, average and peak current;
4. exact DX-LR30 part/revision and authoritative module pin drawing;
5. EU868 versus the actual deployed regional frequency plan;
6. enclosure dimensions, antenna placement, cable glands and mounting holes;
7. final GPIO assignment after schematic electrical-rule review;
8. battery threshold policy and minimum winter autonomy;
9. required outdoor surge/ESD level and cable length;
10. production quantity, preferred suppliers and acceptable substitutions.

Do not release Gerbers until these items and the energy budget are reviewed.
