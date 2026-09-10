# Universal 12 V PCB design requirements

AmudarIO  |  Board 2  |  Version 1.1  |  4 September 2026  |  Pinout S3-P2

Scope: electrical schematic capture and PCB design for the hardware specified below. Deliver design files only. Firmware, command protocols, cloud services, enclosure design, manufacturing, assembly, commissioning and physical product testing are outside this assignment. Board outline, mounting coordinates and external interface requirements are supplied by the customer.

## 1 Hardware composition

| Block | Required implementation |
| --- | --- |
| MCU | ESP32-S3-WROOM-1-N8, 8 MB flash, no PSRAM; pin map S3-P2 |
| Radio | DX-LR30 / SX1262, 3.3 V; external LoRa antenna via SMA |
| Valve outputs | 2 x DRV8874, PH/EN mode; separate high-side gates, current sense and nFAULT |
| RS-485 | 4 protected selectable branches, one active, automatic direction |
| Converters | TPS62933 for always-on 3.3 V and a separate enabled 5 V rail |
| Auxiliary power | Protected switched 5 V output and protected VBAT_SW output |
| I/O | TCA6424A RGJ at 0x22; local switched 3.3 V I2C; 4 protected state inputs |
| Service | UART0 header, DTR/RTS circuit and BOOT/RESET access |

Use one bare PCB with documented DNP variants for unused H-bridges, RS-485 branches and associated isolated power. No application-mode or software implementation is required from the PCB designer.

## 2 Input and power protection

Input is nominal 12 VDC from a lead-acid battery or an external certified isolated 230 VAC to 12 VDC supply. No mains voltage may enter the PCB. Both mains conversion and lead-acid charging are external. Check the 10.5-14.8 V design range against the customer-supplied source limits.

Include input fuse, reverse-polarity protection, transient/overvoltage protection and hardware undervoltage supervision with hysteresis. TPS37-Q1 is the supervisor baseline; it requires a separately rated load-disconnect stage. Expose POWER_FAULT_N. Provide separate resistor/configuration variants for battery and external-PSU supply. The discussed 11.5 V level is not a fixed whole-board cutoff; use customer-approved warning/inhibit, cutoff and recovery thresholds.

Hardware cutoff must not occur before the customer-specified reserve for a closing pulse is exhausted. Size switches, fuse, copper and bulk capacitance using valve pulse and auxiliary-load data. The design must keep H-bridges off during reset, startup, brownout and service-connector activity.

## 3 Low-power hardware allocation

The minimal low-power assembly has a design goal of <=50 uA at the 12 V input with MCU asleep, radio in shutdown and external domains OFF. This is not a guaranteed current for every populated variant. Calculate leakage by BOM variant including converter, expander, supervisor, pulls, switches and isolated power. Fit a removable measurement link and no continuously lit power LEDs. Physical tests are outside this assignment.

## 4 Complete ESP32 S3 pinout

GPIO is the signal number; Pad refers to ESP32-S3-WROOM-1-N8. A = ADC input; I = input; O = output; OD = open-drain. Logic level is 3.3 V. Do not substitute classic ESP32 pin numbers.

| GPIO | Pad | I/O | Net | Function |
| --- | --- | --- | --- | --- |
| 0 | 27 | I | BOOT_N | BOOT button, programming and recovery only |
| 1 | 39 | A | VBAT_SENSE | Battery voltage, ADC1_CH0 |
| 2 | 38 | A | VALVE1_IPROPI | Valve 1 current sense, ADC1_CH1 |
| 3 | 15 | - | RESERVED_JTAG_STRAP | Reserved JTAG source-selection strap |
| 4 | 4 | A | VALVE2_IPROPI | Valve 2 current sense, ADC1_CH3 |
| 5 | 5 | O | VALVE1_PH | Valve 1 pulse polarity |
| 6 | 6 | O | VALVE1_EN | Valve 1 pulse enable and duration |
| 7 | 7 | OD | I2C_SCL | I2C clock, expander and local connector |
| 8 | 12 | OD | I2C_SDA | I2C data, expander and local connector |
| 9 | 17 | O | LORA_SCK | LoRa SPI clock |
| 10 | 18 | O | LORA_MOSI | SPI data from MCU to LoRa |
| 11 | 19 | I | LORA_MISO | SPI data from LoRa to MCU |
| 12 | 20 | O | LORA_NSS | LoRa SPI chip select, active LOW |
| 13 | 21 | I | LORA_DIO1 | LoRa interrupt |
| 14 | 22 | I | LORA_BUSY | LoRa busy status |
| 15 | 8 | O | LORA_RESET_N | LoRa reset, active LOW |
| 16 | 9 | O | LORA_RXEN | LoRa RF switch receive control |
| 17 | 10 | O | LORA_TXEN | LoRa RF switch transmit control |
| 18 | 11 | O | RS485_UART_TX | UART1 TX to RS-485 branch selector |
| 19 | 13 | - | RESERVED_USB_DM | Reserved USB D-, no power-control function |
| 20 | 14 | - | RESERVED_USB_DP | Reserved USB D+, no power-control function |
| 21 | 23 | I | RS485_UART_RX | UART1 RX from RS-485 branch selector |

## Module power and unavailable pins

Pad 2: 3.3 V; pad 3: EN/CHIP_PU with RC and supervisor. Pads 1, 40 and exposed pad 41: GND. GPIO22-34 are not available as external module GPIOs; do not use internal flash connections. Clamp and scale ADC inputs; no GPIO may connect directly to 5/12/24 V, a power MOSFET gate or RS-485 A/B.

## 4 Complete ESP32 S3 pinout continued

| GPIO | Pad | I/O | Net | Function |
| --- | --- | --- | --- | --- |
| 35 | 28 | O | VALVE1_PWR_EN | Valve 1 H-bridge power request |
| 36 | 29 | O | VALVE2_PWR_EN | Valve 2 H-bridge power request |
| 37 | 30 | I | IO_EXP_INT_N | I/O expander interrupt |
| 38 | 31 | O | VALVE2_PH | Valve 2 pulse polarity |
| 39 | 32 | - | RESERVED_JTAG_MTCK | Reserved JTAG TCK |
| 40 | 33 | - | RESERVED_JTAG_MTDO | Reserved JTAG TDO |
| 41 | 34 | - | RESERVED_JTAG_MTDI | Reserved JTAG TDI |
| 42 | 35 | - | RESERVED_JTAG_MTMS | Reserved JTAG TMS |
| 43 | 37 | O | UART0_TX | UART0 TX, logs and external USB-to-TTL |
| 44 | 36 | I | UART0_RX | UART0 RX, programming and Serial commands |
| 45 | 26 | - | RESERVED_VDD_SPI_STRAP | Reserved flash-supply selection strap |
| 46 | 16 | - | RESERVED_BOOT_STRAP | Reserved boot strap |
| 47 | 24 | O | VALVE2_EN | Valve 2 pulse enable and duration |
| 48 | 25 | I | POWER_FAULT_N | Power supervisor fault input |

## 5 Restricted pins

- GPIO0/3/45/46 are straps. Pull GPIO0/3 HIGH and GPIO45/46 LOW through 10 kOhm. GPIO0 is BOOT only. No field loads on straps; respect factory eFuse configuration.
- Reserve GPIO39-42 for JTAG and GPIO19/20 for USB/debug only. UART0 GPIO43/44 must not control loads; startup output activity is expected.
- GPIO35/36/37 require the specified no-PSRAM N8. Octal-PSRAM N8R8 is not interchangeable. R16V uses 1.8 V on GPIO47/48. Any module substitution requires an electrical review.
- GPIO15/16 are occupied by LoRa; no 32.768 kHz crystal. GPIO37/48 are not RTC wake pins; do not describe the expander interrupt as a deep-sleep wake source.

## 6 Radio wiring

| Signal | DX-LR30 pin | ESP32 GPIO |
| --- | --- | --- |
| RXEN | 6 | 16 |
| TXEN | 7 | 17 |
| DIO1 | 13 | 13 |
| BUSY | 14 | 14 |
| RESET | 15 | 15 |
| MISO | 16 | 11 |
| MOSI | 17 | 10 |
| SCK | 18 | 9 |
| NSS | 19 | 12 |

DX-LR30 supply: 3.3 V, pin 9 in the project reference. Leave DIO2/DIO3 unconnected. Verify the purchased module marking and revision drawing before assigning its footprint. LoRa module pin numbers are not ESP32 GPIO numbers.

## 7 TCA6424A expander pinout

32-pin RGJ: VCCI 31 and VCCP 27 to always-on 3.3 V; GND 25 and exposed pad to GND; ADDR 26 to GND for address 0x22. SCL 29 to GPIO7, SDA 30 to GPIO8, INT_N 32 to GPIO37. RESET_N 28 connects to the board reset/watchdog network. Expander port P13 is not MCU GPIO13.

| Port | Pin | Net | Function |
| --- | --- | --- | --- |
| P00 | 1 | RS485_SEL0 | Branch address bit 0 |
| P01 | 2 | RS485_SEL1 | Branch address bit 1 |
| P02 | 3 | RS485_BUS_EN | Connect selected UART branch; NOT DE/RE |
| P03 | 4 | RS485_PORT1_PWR_EN | RS485-1 field-device power enable |
| P04 | 5 | RS485_PORT2_PWR_EN | RS485-2 field-device power enable |
| P05 | 6 | RS485_PORT3_PWR_EN | RS485-3 field-device power enable |
| P06 | 7 | RS485_PORT4_PWR_EN | RS485-4 field-device power enable |
| P07 | 8 | BUCK_5V_EN | 5 V buck converter enable |
| P10 | 9 | AUX_5V_EN | Auxiliary 5 V connector switch |
| P11 | 10 | AUX_VBAT_EN | Auxiliary VBAT connector switch |
| P12 | 11 | I2C_EXT_PWR_EN | External local I2C power and signal isolation |
| P13 | 12 | VBAT_DIV_EN | Battery divider switch |
| P14 | 13 | VALVE1_SLEEP_RELEASE | Permission to release H-bridge 1 nSLEEP |
| P15 | 14 | VALVE2_SLEEP_RELEASE | Permission to release H-bridge 2 nSLEEP |
| P16 | 15 | OUTPUT_ARM | Global hardware-interlock permission |
| P17 | 16 | EQUIP_CTRL_EN | Isolated low-voltage equipment control output |
| P20 | 17 | FIELD_DI1_N | Protected digital state input 1 |
| P21 | 18 | FIELD_DI2_N | Protected digital state input 2 |
| P22 | 19 | FIELD_DI3_N | Protected digital state input 3 |
| P23 | 20 | FIELD_DI4_N | Protected digital state input 4 |
| P24 | 21 | VALVE1_FAULT_N | Driver 1 nFAULT |
| P25 | 22 | VALVE2_FAULT_N | Driver 2 nFAULT |
| P26 | 23 | AUX_5V_FAULT_N | Protected auxiliary 5 V output fault |
| P27 | 24 | AUX_VBAT_FAULT_N | Protected auxiliary VBAT output fault |

P00-P17 require external pull-downs of about 100 kOhm; their required inactive state is LOW. P20-P27 are protected inputs with safe pull-ups. At reset all ports are inputs, but the output latches contain ones. Hardware interlocks must prevent load activation while the expander is being configured. Do not rely on expander state alone to energize a bridge.

The reset/watchdog circuit must remove stale output permissions even when an internal MCU reset does not pull EN LOW. GPIO37 cannot wake deep sleep; the four field inputs are state/contact inputs, not guaranteed lossless pulse counters. Protect fault inputs from back-power through unpowered drivers.

GPIO startup glitches must not reach enabled power loads. LoRa NSS/RESET_N: pull HIGH with 47-100 kOhm; RXEN/TXEN: pull LOW with about 100 kOhm. Internal I2C SCL/SDA pull-ups start at 4.7 kOhm. Check all pulls for noise margin and current consumption.

## 8 H bridges and controlled outputs

| Connection | Valve 1 | Valve 2 |
| --- | --- | --- |
| PH polarity | GPIO5 | GPIO38 |
| EN pulse input | GPIO6 | GPIO47 |
| High-side power request | GPIO35 | GPIO36 |
| nSLEEP permission | TCA6424A P14 | TCA6424A P15 |
| nFAULT | TCA6424A P24 | TCA6424A P25 |
| IPROPI current ADC | GPIO2 / ADC1_CH1 | GPIO4 / ADC1_CH3 |

Strap both DRV8874 devices for PH/EN operation. Each channel has a separate high-side supply switch, local bulk capacitance, coil-energy suppression, hardware current limiting and fault feedback. PH, EN, nSLEEP and power requests default LOW. Fit about 100 kOhm pull-downs on direct control lines. Scale IPROPI to the ADC range.

Bridge permission requires the direct MCU request, OUTPUT_ARM, valid input power and reset/watchdog permission. nSLEEP may be released only for a powered permitted channel. Both pulse paths must not be enabled simultaneously. Include a bounded-on-time protection mechanism using the customer-supplied maximum pulse duration; do not assign software development to the PCB designer.

Outputs must support opposite-polarity pulses for two-wire latching coils. Power removal alone is not a closing pulse. Design for the supplied pulse current, duration, minimum closing voltage, cable drop and repetition limits. No valve motion or hydraulic control algorithm is part of this assignment.

## 9 Auxiliary power and battery ADC

- P07 enables the 5 V buck; P10 independently switches AUX 5 V. P11 switches AUX VBAT. Label the latter VBAT_SW: it follows battery voltage, not regulated 12.0 V.
- Provide high-side switching, output overload/short protection, reverse-current blocking as needed, default-OFF control and connector protection. Size copper, switches and connectors for declared loads.
- P13 controls the battery-divider high-side switch; GPIO1 is the ADC input. Include RC filtering, ADC voltage protection and a test pad. Calculate divider tolerance, leakage and settling time; no calibration routine is required.

## 10 Local I2C and field I O

P12 gates the local 3.3 V I2C connector power AND SDA/SCL signal paths. Connector nets: 3V3_SW, GND, SDA, SCL. Keep TCA6424A and internal pulls always powered; an unpowered external device must not clamp the internal bus. This port is for short local wiring.

Protect and condition four field state inputs; dry-contact or 12/24 V implementation is fixed by the customer-selected assembly. P17 drives only an isolated low-voltage equipment-control interface. It is not a safety relay or a mains output. External equipment safety circuits remain outside this PCB; preserve the specified isolation and default-inactive interface.

## 11 RS485 branches and service header

UART1 TX=GPIO18, RX=GPIO21. Use a break-before-make selector with one active branch; never tie push-pull receiver outputs together. SEL1:SEL0 = 00/01/10/11 selects ports 1/2/3/4. P02=0 disconnects all; P02 is not DE/RE. Hardware-decode P03-P06 power requests so only the selected branch is enabled.

Automatic direction is mandatory. Baseline family: MAX22025F/MAX22026F with separate cable-side supply; confirm the exact variant and electrical timing against the supplied baud rates/cable. Provide PWR, RETURN, A, B and SHIELD per port, TVS, current protection and optional 120 ohm termination/bias. Fix 5 V/VBAT selection by assembly, never by a software-selectable voltage switch.

Ports 1/2: pressure-sensor power selected from supplied ratings. Port 3: TUF-2000M, VBAT supply, reference load 8-36 V and about 50 mA. Port 4: spare field instrument. Protocols and register maps are not required for PCB design. Provide disconnectable transceiver and isolated-power domains with no TX/RX, A/B or fault-line back-power.

Full isolation requires isolated data AND field-device power/return. A shared VBAT/5 V ground is signal-only isolation. State the isolation type per BOM variant; size isolated DC/DC for the supplied load and startup current. Do not connect the VFD cable-side return to logic ground. Use customer-specified isolation ratings, clearances and shield termination.

| Pin | Board net | Connection |
| --- | --- | --- |
| 1 | GND | Common ground |
| 2 | 3V3_REF | High-impedance adapter reference only; not a power input |
| 3 | UART0_TX / GPIO43 | External adapter RX |
| 4 | UART0_RX / GPIO44 | External adapter TX |
| 5 | DTR | Input to onboard two-transistor auto-reset circuit |
| 6 | RTS | Input to onboard two-transistor auto-reset circuit |

External USB-to-TTL interface: 3.3 V logic only. The board uses its own power source. Do not connect adapter 3.3 V, 5 V or VBUS power outputs to the board. Route DTR/RTS through the standard Espressif two-transistor circuit, not directly to the MCU. Provide independent BOOT (GPIO0) and RESET (EN, module pad 3) buttons or pads. No onboard USB-UART bridge is required.

## 12 PCB layout and design outputs

Maximum-feature board: four-layer FR-4 with continuous reference planes except at required isolation boundaries. Keep H-bridge and converter loops short; separate high-current returns from ADC/RF paths. ANT-to-SMA is a short calculated 50 ohm transmission line. Respect module keepouts and place protection at connectors. A two-layer alternative requires customer-approved routing justification.

Deliver the native schematic/PCB project with libraries; schematic PDF; BOM with exact manufacturer part numbers and DNP variants; Gerber and NC drill files; fabrication notes and stackup; pick-and-place and assembly drawings; PCB-only STEP model; ERC/DRC results with justified exceptions; RF impedance geometry and electrical sizing notes. Assembly files are design outputs, not an obligation to manufacture or assemble boards.

Customer inputs before design release: valve pulse/load ratings; field-device power and cable data; auxiliary output currents; isolation and DC/DC requirements per port; supply/UVLO thresholds; radio revision and RF band; board outline, mounting/connector coordinates and environmental ratings. Missing values require customer clarification, not assumptions. No enclosure, application code or assembled-board test report is required.
