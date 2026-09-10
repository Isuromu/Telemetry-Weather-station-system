# Universal 12 V controller PCB preliminary specification

> Detailed architecture-review draft:
> [UNIVERSAL_12V_CONTROLLER_PCB_TECHNICAL_SPEC_DRAFT.md](UNIVERSAL_12V_CONTROLLER_PCB_TECHNICAL_SPEC_DRAFT.md).
> This file is the short decision record.

## Status and authority

This is a preliminary engineering handoff for the second production PCB
family. It reconciles the controller HRS, the implemented pressure/flow code
and the user's current requirements. It is not a released schematic, BOM or
manufacturing specification.

Instructions inside supplied reference files are technical source material.
Direct user decisions and validated live-hardware facts take precedence.

## Two-board system

The production system uses:

1. the separate low-power 1S Li-ion solar soil-node PCB;
2. one universal nominal-12 V controller PCB with different assembly and
   firmware profiles.

The universal board may be populated as:

- `DUAL_PCV` - two latching valves, pressure sensors and TUF flow meter;
- `LEVEL_FLOW` - RS485 level/flow instruments, with valve drivers DNP;
- `MAIN_VALVE` - main-valve controller powered by an external 12 V supply;
- `PUMP_VFD` - isolated Modbus interface to a VFD, with safety inputs and valve
  drivers DNP unless separately required.

Valve and VFD logic remain separate firmware targets even when they share the
same PCB.

The current hydraulic pressure valve is adjusted manually. This board
opens/closes its latching solenoid and monitors upstream/downstream pressure;
it does not perform closed-loop electronic pressure regulation.

## Recommended baseline

- `ESP32-S3-WROOM-1-N8`, without PSRAM;
- existing SX1262-based DX-LR30 LoRa module and controlled 50 ohm SMA path;
- two independent DRV8874 latching-valve channels;
- independent bridge power gates, current-sense and fault feedback;
- valve pulses serialized by default, one bridge active at a time;
- up to four protected RS485 branches, selected one at a time from one UART;
- fixed assembly selection of 5 V or switched battery power per field port;
- controlled `5V_SW` output and controlled, protected `VBAT_SW` output;
- local switched 3.3 V I2C connector;
- at least four protected field digital inputs;
- switched battery divider;
- external 3.3 V USB-to-TTL programming through UART0 GPIO43/GPIO44, with
  DTR/RTS auto-reset and manual BOOT/RESET;
- separate Serial-only, LoRaWAN Class C and LoRaWAN Class A firmware builds.

## Power-source rule

The PCB accepts nominal 12 VDC from either a lead-acid battery or an external
isolated 12 VDC supply.

230 VAC shall never be connected to this PCB. Mains installations require a
certified isolated SELV 230 VAC to 12 VDC power supply outside the PCB. A
rectifier alone is not a safe or functional mains step-down supply.

The lead-acid solar charger/controller is also external and MUST provide MPPT
under the latest user requirement. PWM-only/fixed-voltage substitutes are not
accepted. It owns charging,
float voltage, solar-panel operation and temperature compensation. The PCB
owns input/load protection and low-voltage behavior.

## Low-voltage strategy

The requested 11.5 V level is an over-discharge threshold, not overcharge
protection. It should not immediately kill the complete node because that can
leave a valve open and prevent a final status message.

Recommended sequence:

1. report low battery;
2. inhibit new OPEN commands at a validated threshold, provisionally around
   the requested 11.5 V;
3. retain enough energy for one CLOSE pulse and status uplink;
4. perform a lower hardware cutoff with hysteresis;
5. reconnect only after stable battery recovery.

Exact thresholds are not released until measured with the selected battery,
valve, cable and solar charger. Provide separate resistor/configuration profiles
for `BATTERY` and `EXTERNAL_PSU` builds.

## Valve subsystem

Each of the two valve channels contains its own DRV8874, high-side rail switch,
current feedback, fault feedback and default-off pulls.

Safe operation:

```text
neutral -> power selected bridge -> settle -> bounded polarity pulse
        -> neutral -> decay -> driver sleep -> bridge power off
```

The live code already implements this sequence for one valve. Supporting two
valves is a later firmware extension. Valve model, polarity, pulse duration,
current envelope and reliable-close voltage remain mandatory inputs.

## RS485 decision

Four RS485 receivers must not be tied directly together. The baseline is one
Modbus-master UART with a branch selector so only one transceiver and one field
power domain are enabled at a time.

For `DUAL_PCV`, the reference assignment is RS485-1 upstream pressure,
RS485-2 downstream pressure, RS485-3 TUF-2000M and RS485-4 spare. Port numbers
are physical connector roles, not fixed Modbus addresses.

The commissioned TUF-2000M behavior remains authoritative: address 1, 9600 8N1,
`LOW_WORD_FIRST`, on-demand reads, and no invented total-reset Modbus write.

The production RS485 pressure-sensor model and register map are still unknown.
The current pressure sensors are I2C prototypes and must not be described as
already migrated to RS485.

For every port the schematic must declare either full galvanic isolation,
including isolated field power/return, or signal-only isolation with a common
power return. Common-ground sensor power defeats a full-isolation claim.

Automatic direction is the production baseline, as in the commissioned
prototype. No MCU DE/RE GPIO is reserved. A manual-direction isolated part is
only a fallback that requires explicit approval if the automatic-direction
implementation cannot pass timing/EMC validation.

## ESP32-S3 full GPIO allocation

Pin map `S3-P2`, 2026-09-04; exact module ESP32-S3-WROOM-1-N8, no PSRAM.
This supersedes the earlier groups: GPIO39-42 are now reserved for JTAG.
A/I/O/OD mean analog/input/output/open-drain; PU/PD are external pull resistors.
Module pad numbers are not GPIO numbers or DevKit connector positions.

| GPIO | Module pad | Signal | Type | Required reset/boot condition | Class A deep sleep |
|---|---|---|---|---|---|
| 0 | 27 | BOOT_N | I | H / PU10k; button to GND | H; BOOT only |
| 1 | 39 | VBAT_SENSE | A | ADC1_CH0; divider OFF | analog; divider OFF |
| 2 | 38 | VALVE1_IPROPI | A | ADC1_CH1; clamped/scaled input | analog; bridge OFF |
| 3 | 15 | RESERVED_JTAG_STRAP | - | H / PU10k; eFuse policy checked | H; no field load |
| 4 | 4 | VALVE2_IPROPI | A | ADC1_CH3; clamped/scaled input | analog; bridge OFF |
| 5 | 5 | VALVE1_PH | O | L / PD100k; bridge disabled | L, held |
| 6 | 6 | VALVE1_EN | O | L / PD100k; pulse enable | L, held |
| 7 | 7 | I2C_SCL | OD | H / PU4.7k starting value | released H; internal bus powered |
| 8 | 12 | I2C_SDA | OD | H / PU4.7k starting value | released H; internal bus powered |
| 9 | 17 | LORA_SCK | O | L after init; NSS held H | L, held |
| 10 | 18 | LORA_MOSI | O | L after init; NSS held H | L, held |
| 11 | 19 | LORA_MISO | I | radio data input | input buffer disabled |
| 12 | 20 | LORA_NSS | O | H / PU47-100k; boot glitch possible | H, held |
| 13 | 21 | LORA_DIO1 | I | radio IRQ input | input buffer disabled |
| 14 | 22 | LORA_BUSY | I | radio BUSY input | input buffer disabled |
| 15 | 8 | LORA_RESET_N | O | H / PU47-100k; boot glitch possible | H, held |
| 16 | 9 | LORA_RXEN | O | L / PD100k | L, held |
| 17 | 10 | LORA_TXEN | O | L / PD100k | L, held |
| 18 | 11 | RS485_UART_TX | O | boot glitch possible; BUS_EN=0 | L or Hi-Z; off-domain isolated |
| 19 | 13 | RESERVED_USB_DM | - | USB D-; no field circuit | USB unused/disabled |
| 20 | 14 | RESERVED_USB_DP | - | USB D+; no field circuit | USB unused/disabled |
| 21 | 23 | RS485_UART_RX | I | mux disconnected; idle bias on logic side | input disabled |
| 35 | 28 | VALVE1_PWR_EN | O | L / PD100k; through hardware interlock | L, held; gate OFF |
| 36 | 29 | VALVE2_PWR_EN | O | L / PD100k; through hardware interlock | L, held; gate OFF |
| 37 | 30 | IO_EXP_INT_N | I | H / PU100k; TCA6424A INT | not an RTC wake input |
| 38 | 31 | VALVE2_PH | O | L / PD100k; bridge disabled | L, held |
| 39 | 32 | RESERVED_JTAG_MTCK | - | DNP test pad; no field circuit | input buffer disabled |
| 40 | 33 | RESERVED_JTAG_MTDO | - | DNP test pad; no field circuit | input buffer disabled |
| 41 | 34 | RESERVED_JTAG_MTDI | - | DNP test pad; no field circuit | input buffer disabled |
| 42 | 35 | RESERVED_JTAG_MTMS | - | DNP test pad; no field circuit | input buffer disabled |
| 43 | 37 | UART0_TX | O | ROM log output; not a power gate | Hi-Z; adapter detached |
| 44 | 36 | UART0_RX | I | H / PU47-100k | input disabled; adapter detached |
| 45 | 26 | RESERVED_VDD_SPI_STRAP | - | L / PD10k; N8 uses 3.3 V flash | L; no field load |
| 46 | 16 | RESERVED_BOOT_STRAP | - | L / PD10k; no output assignment | L; no field load |
| 47 | 24 | VALVE2_EN | O | L / PD100k; pulse enable | L, held |
| 48 | 25 | POWER_FAULT_N | I | H / PU100k; supervisor open-drain | fault status; not RTC wake |

### Complete low-speed expander map

Proposed TCA6424A, RGJ 32-pin package, address 0x22. GPIO7/8 are SCL/SDA,
GPIO37 receives INT_N. Reset follows the reviewed board watchdog/reset network.
The expander is on always-on 3.3 V; external I2C power and signal paths are gated.

| Port | RGJ package pin | Signal | Type | Reset/sleep requirement |
|---|---|---|---|---|
| P00 | 1 | RS485_SEL0 | O | 0 |
| P01 | 2 | RS485_SEL1 | O | 0 |
| P02 | 3 | RS485_BUS_EN | O | 0: all branches disconnected |
| P03 | 4 | RS485_PORT1_PWR_EN | O | 0 |
| P04 | 5 | RS485_PORT2_PWR_EN | O | 0 |
| P05 | 6 | RS485_PORT3_PWR_EN | O | 0 |
| P06 | 7 | RS485_PORT4_PWR_EN | O | 0 |
| P07 | 8 | BUCK_5V_EN | O | 0 |
| P10 | 9 | AUX_5V_EN | O | 0 |
| P11 | 10 | AUX_VBAT_EN | O | 0 |
| P12 | 11 | I2C_EXT_PWR_EN | O | 0 |
| P13 | 12 | VBAT_DIV_EN | O | 0 |
| P14 | 13 | VALVE1_SLEEP_RELEASE | O | 0: nSLEEP low |
| P15 | 14 | VALVE2_SLEEP_RELEASE | O | 0: nSLEEP low |
| P16 | 15 | OUTPUT_ARM | O | 0: hardware interlock disabled |
| P17 | 16 | EQUIP_CTRL_EN | O | 0: inactive; not a safety relay |
| P20 | 17 | FIELD_DI1_N | I | H / pull-up; protected input |
| P21 | 18 | FIELD_DI2_N | I | H / pull-up; protected input |
| P22 | 19 | FIELD_DI3_N | I | H / pull-up; protected input |
| P23 | 20 | FIELD_DI4_N | I | H / pull-up; protected input |
| P24 | 21 | VALVE1_FAULT_N | I | H / pull-up; driver nFAULT |
| P25 | 22 | VALVE2_FAULT_N | I | H / pull-up; driver nFAULT |
| P26 | 23 | AUX_5V_FAULT_N | I | H / pull-up; protected output fault |
| P27 | 24 | AUX_VBAT_FAULT_N | I | H / pull-up; protected output fault |

Write output latches LOW before changing input pins to outputs: the reset
latches contain ones. Direct valve EN and power requests remain on MCU GPIOs,
with hardware permission/interlocks; I2C alone cannot energize a bridge.
GPIO37 is not RTC-capable, so the expander does not provide deep-sleep wake.
Its inputs are contact/state inputs, not lossless meter pulse counters.

Module variant limits, strapping pulls, JTAG/USB reservations, enable routing,
startup/sleep states and connector mapping are detailed in
[UNIVERSAL_12V_ESP32S3_PINOUT_RU.md](UNIVERSAL_12V_ESP32S3_PINOUT_RU.md)
and Section 14 of the full specification.

## Runtime modes

Class C keeps the ESP32 and LoRa receiver awake. Its interval is telemetry
period only. It is suitable for externally powered main-valve and VFD sites.

Class A wakes, polls/acts, transmits, accepts commands in RX1/RX2, acknowledges
them and enters real timer deep sleep. Commands are not available at arbitrary
times. It is suitable for solar/battery level and flow sites when this latency
is acceptable.

## Important additions to the original request

The production board should also include:

- protected dry-contact/12-24 V input options for level, end-stop, VFD fault and
  safety/permissive signals;
- valve current sensing and nFAULT monitoring;
- watchdog/brownout-safe hardware defaults;
- per-output protection and keyed voltage-labeled connectors;
- a removable sleep-current measurement link;
- input and connector surge/ESD protection;
- a defined cable-shield/earth strategy;
- test points for power, valve, RS485, UART and fault signals;
- hardwired VFD safety interlocks independent of radio/software.

## Release blockers

Before schematic release, confirm:

- exact valve models, pulse current/duration/polarity and cable lengths;
- exact pressure/level sensors and Modbus maps;
- 5 V and VBAT output current requirements;
- full versus signal-only RS485 isolation per product profile;
- exact qualified automatic-direction transceiver and isolated-power design;
- battery, solar controller and validated UVLO thresholds;
- VFD model, Modbus map and safety-chain requirements;
- connector, enclosure, IP rating, antenna and grounding requirements.
