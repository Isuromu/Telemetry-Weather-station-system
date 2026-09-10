# Universal 12 V controller PCB technical specification

AmudarIO  |  Board 2  |  Version 1.0  |  4 September 2026  |  Pinout S3-P2

Design one PCB with assembly variants for two latching valves, RS-485 pressure/flow/level sensors, and a low-voltage main-valve or variable-frequency-drive interface. The board opens and closes valves and measures pressure; closed-loop electronic pressure regulation is not included.

## 1 Hardware and application profiles

| Block | Implementation |
| --- | --- |
| MCU | ESP32-S3-WROOM-1-N8, 8 MB flash, no PSRAM |
| LoRa | SX1262-based DX-LR30, SMA, LoRaWAN Class A or Class C |
| Valves | 2 x DRV8874 in PH/EN mode; independent power gates, current sense and nFAULT |
| RS-485 | 4 protected selectable branches; one active; automatic direction |
| Power | Always-on 3.3 V and switched 5 V using TPS62933; protected VBAT_SW |
| Additional I/O | TCA6424A RGJ at 0x22; local 3.3 V I2C; 4 protected state inputs |
| Service | External USB-to-TTL, UART0, DTR/RTS, BOOT/RESET |

| Profile | Purpose | Mode |
| --- | --- | --- |
| DUAL_PCV | Two valves, upstream/downstream pressure, TUF-2000M | Class A or C |
| LEVEL_FLOW | Level/flow sensors; H-bridges not populated | Usually Class A |
| MAIN_VALVE | Main valve, external 12 V supply | Usually Class C |
| PUMP_VFD | VFD RS-485, status and interlock inputs | Class C |

## 2 Input power and battery protection

Accept DC only: a 12 V lead-acid battery or an external certified isolated 230 VAC to 12 VDC supply. No mains voltage may enter this PCB. The lead-acid solar charger is external. Check the 10.5-14.8 V design operating range against the selected source.

Provide an input fuse, reverse-polarity and transient/overvoltage protection, hardware UVLO with hysteresis, and POWER_FAULT_N monitoring. TPS37-Q1 is the supervisor baseline, not a power switch. On discharge, warn and inhibit new OPEN commands first, reserve energy for CLOSE/status, then apply the lower hardware cutoff. The suggested 11.5 V is a starting point for validating OPEN inhibit, not a fixed whole-board cutoff. Calculate thresholds and delays separately for BATTERY and EXTERNAL_PSU.

## 3 Complete ESP32 S3 pinout

GPIO is the firmware number. Pad refers to ESP32-S3-WROOM-1-N8. A = ADC input; I = input; O = output; OD = open-drain. All signals use 3.3 V logic. S3-P2 does not describe the classic ESP32 prototype.

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

## Module power

Pad 2: 3.3 V; pad 3: EN/CHIP_PU with RC and supervisor. Pads 1, 40 and exposed pad 41: GND. GPIO22-34 are not available as external module GPIOs; do not use internal flash connections.

Assign SPI and UART explicitly through the GPIO matrix. Do not connect any GPIO directly to 5/12/24 V, a power MOSFET gate or RS-485 A/B. Clamp and scale analog inputs to the permitted ADC range.

## 3 Complete ESP32 S3 pinout continued

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

## 4 Pin restrictions

- GPIO0/3/45/46 are straps. Pull GPIO0/3 HIGH through 10 kOhm and GPIO45/46 LOW through 10 kOhm. GPIO0 is BOOT only. Do not attach field loads; check factory eFuses.
- Reserve GPIO39-42 exclusively for JTAG. Reserve GPIO19/20 for USB/debug, not power enables. GPIO43/44 are dedicated UART0; ROM logs must not activate loads.
- GPIO35/36/37 are available on the selected no-PSRAM N8. An octal-PSRAM N8R8 substitution changes pin availability. R16V is also not interchangeable: GPIO47/48 operate at 1.8 V. Any module substitution requires a schematic review.
- GPIO15/16 are used by LoRa; do not fit a 32.768 kHz crystal. GPIO37/48 are not RTC inputs; baseline Class A uses timer wake.

## 5 LoRa connections

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

## 6 TCA6424A expander pinout

Package: 32-pin RGJ. VCCI pin 31 and VCCP pin 27: always-on 3.3 V; GND pin 25 and exposed pad: ground; ADDR pin 26: GND, address 0x22. SCL pin 29: GPIO7; SDA pin 30: GPIO8; INT_N pin 32: GPIO37. Connect RESET_N pin 28 to the common reset/watchdog network. Port P13 is not GPIO13.

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

P00-P17 require external pull-downs of about 100 kOhm and become outputs after initialization; P20-P27 are protected inputs with pull-ups. Reset makes all ports inputs, but output latches contain ones. First write and verify 0x00 in 0x04/0x05/0x06; then set Configuration 0x0C=0x00, 0x0D=0x00, 0x0E=0xFF. Enable OUTPUT_ARM last, after direct GPIOs are safe.

An MCU restart must not preserve unsafe expander permissions. Internal MCU reset does not necessarily pull EN LOW; hardware interlocks and watchdog must cover this case. GPIO37 cannot wake the MCU from deep sleep. The four state inputs are for contacts, not guaranteed lossless flow-meter pulse counting.

GPIOs can glitch at startup: inhibit H-bridges and RS-485 branches in hardware. Pull LoRa NSS/RESET_N HIGH through 47-100 kOhm and RXEN/TXEN LOW through 100 kOhm; reinitialize the radio after reset. Start with 4.7 kOhm I2C SCL/SDA pull-ups. Verify all pulls for noise and leakage.

## 7 Two independent valve channels

| Function | Valve 1 | Valve 2 |
| --- | --- | --- |
| PH polarity | GPIO5 | GPIO38 |
| EN pulse | GPIO6 | GPIO47 |
| H-bridge power request | GPIO35 | GPIO36 |
| nSLEEP permission | TCA6424A P14 | TCA6424A P15 |
| nFAULT | TCA6424A P24 | TCA6424A P25 |
| IPROPI current ADC | GPIO2 / ADC1_CH1 | GPIO4 / ADC1_CH3 |

Configure both DRV8874 devices in hardware for PH/EN mode. Supply each H-bridge through its own high-side switch only during an operation. Hardware permissions must include OUTPUT_ARM, valid power and reset/watchdog. Release nSLEEP only for a powered channel. Fit external pull-downs of about 100 kOhm on PH, EN and power requests. An I2C command alone must not energize a bridge.

Sequence: EN=0; enable selected bridge power; release nSLEEP; wait for stabilization; set PH; generate a bounded EN pulse; EN=0; wait for current decay; nSLEEP=0; power OFF. Actuate valves one at a time. Maximum-pulse supervision and shutdown on a hang must prevent continuous coil energization.

OPEN and CLOSE require opposite polarities. Removing power does not close a latching valve. Determine duration, polarity, current limit and minimum reliable closing voltage using the actual valve. Provide coil-energy handling and short/open-load/overtemperature protection. Without position feedback, record the last issued action only; physical position remains unconfirmed.

## 8 Controlled outputs and local I2C

- P07 enables the 5 V converter; P10 separately enables protected AUX 5 V. P11 enables protected AUX VBAT. Label the latter VBAT_SW: on battery power it follows battery voltage, not regulated 12.0 V.
- Each output requires a high-side switch, overload/short-circuit and reverse-back-power protection, reset-default OFF behavior and connector protection. Calculate current ratings and protection values from the load, not IC peak ratings.
- P12 enables external I2C power and SDA/SCL isolation. Connector: 3V3_SW, GND, SDA, SCL. Keep the expander and internal pulls on always-on 3.3 V. I2C is for short enclosure-local wiring only.
- P13 enables the divider high-side switch; GPIO1 measures battery voltage. Provide RC filtering, ADC clamping, two-point calibration and divider shutdown after reading. Report voltage, low/inhibit status and reset cause, not an accurate charge percentage.

Protect and level-condition field inputs; select dry-contact or 12/24 V input circuits by assembly profile. P17 controls only an isolated low-voltage equipment permission. It does not replace emergency stop; the VFD safe-stop chain must operate independently of MCU, I2C and LoRa.

## 9 RS485 and field-device power

UART1 TX=GPIO18, RX=GPIO21. Connect four branches through a break-before-make selector; never tie push-pull receiver outputs together. SEL1:SEL0 = 00/01/10/11 selects RS485-1/2/3/4. P02=0 disconnects all branches; it enables the selector and is not DE/RE. P03-P06 request port power; a hardware decoder permits only the selected branch.

| Port | DUAL_PCV role | Power |
| --- | --- | --- |
| RS485-1 | Upstream pressure | 5 V or VBAT according to the sensor |
| RS485-2 | Downstream pressure | 5 V or VBAT according to the sensor |
| RS485-3 | TUF-2000M | VBAT; manual specifies 8-36 V, about 50 mA |
| RS485-4 | Spare level/flow/equipment port | Fixed by assembly profile |

Before switching, finish the transaction, set P02=0 and remove old-branch power. Select the new port, enable its domain, wait for startup, then permit communication. Only one branch is active in the baseline. Fix 5 V/VBAT selection in hardware at assembly; software must not be able to apply VBAT to a 5 V sensor.

Direction is automatic. Use the MAX22025F/MAX22026F isolated-transceiver family as the baseline with a separate cable-side supply. The contractor shall select the exact variant and verify required baud rates, bus release, EMC and shutdown current. Changing to manual DE/RE requires approval.

Full galvanic isolation requires isolation of both signals AND sensor power/return. For a board-powered sensor, use a suitably rated isolated DC/DC; a common VBAT/5 V ground defeats full isolation. Identify FULL_ISOLATION and SIGNAL_ISOLATION_ONLY separately in the BOM. Do not connect the VFD field return to board logic ground.

Each port provides PWR, RETURN, A, B and SHIELD; keying, voltage labels, current protection, TVS, optional 120 ohm termination and bias footprints. Set isolation ratings, clearances and shield termination for the installation. Disable the transceiver and isolated DC/DC outside sessions; prevent back-power through TX/RX, A/B and fault lines.

## 10 Flow-meter compatibility

Retain the commissioned TUF-2000M settings: address 1; 9600 8N1; Modbus RTU; REAL4 LOW_WORD_FIRST. Use function 03 for flow REG0001-0002, velocity REG0005-0006, errors REG0072 and accumulated volumes REG0113-0118. Request addresses are one less than the REG number. A delivered-volume reset stores a baseline in ESP32 NVS; do not modify meter totalizers or menu M37.

Measure TUF power-on readiness and qualify power cycling. An unpowered meter cannot account for passing water; continuous totalization requires a separate always-powered profile and a review of sequential branch power. Select RS-485 pressure sensors using their own manuals; do not reuse the old I2C sensor scaling.

## 11 Programming operating modes and safe sleep

| Pin | Board signal | Adapter connection |
| --- | --- | --- |
| 1 | GND | GND |
| 2 | 3V3_REF | High-impedance reference input only; otherwise leave open |
| 3 | UART0_TX / GPIO43 | Adapter RX |
| 4 | UART0_RX / GPIO44 | Adapter TX |
| 5 | DTR | Through the two-transistor auto-reset circuit |
| 6 | RTS | Through the two-transistor auto-reset circuit |

Use 3.3 V logic only. Power the board from its own source; do not connect the adapter 3.3 V, 5 V or VBUS power outputs. BOOT: GPIO0; RESET: EN, module pad 3. Route DTR/RTS through the standard Espressif circuit, never directly to BOOT/EN. With a TX/RX/GND-only adapter, use the BOOT and RESET buttons. The same UART0 supports Serial Monitor; verify DTR/RTS behavior when opening the port.

Serial-only keeps the MCU awake for local commands and does not start LoRa. Class C keeps the MCU and LoRa receiving; its interval is a telemetry period, not sleep time. Class A uses timer wake, measurement/action, uplink, RX1/RX2, immediate command acknowledgement and real deep sleep; it cannot receive commands at arbitrary times. Keep valve and VFD firmware separate.

Before Class A sleep: EN/PH=0, nSLEEP=0, bridge power requests=0; set P00-P17 to safe OFF; shut down LoRa, RXEN/TXEN=0, NSS/RESET=1, SCK/MOSI=0. Use supported digital-pad deep-sleep hold and external pulls for direct outputs; set safe levels before releasing hold on wake. An I2C error must immediately drop direct EN and bridge power requests.

Design the minimal Class A assembly toward no more than 50 uA; establish acceptance limits for each assembly using maximum leakage of all populated circuits. This is not a guaranteed full-board current. Measure at the 12 V input without a programmer or active sensors; include TCA6424A, pulls, switches, supervisor, LoRa and DC/DC. Class C and Serial-only are not sleep modes. Provide a removable current-measurement link; omit continuously lit LEDs.

## 12 PCB verification and contractor deliverables

Maximum-feature assembly: four-layer FR-4, continuous reference GND respecting isolation clearances, and short separate H-bridge power loops. Route ANT-to-SMA as a calculated 50 ohm line; keep power returns out of RF/ADC paths. A two-layer option requires routing and EMC review. Label connector voltages and polarities.

Acceptance: ERC/DRC; power-up/BOOT/reset/watchdog/brownout without valve pulses; short/open-load/thermal tests per output; no simultaneous valve pulses; reliable OPEN/CLOSE; UVLO/recovery; four RS-485 branches; TUF; LoRaWAN; I2C stuck-low; no back-power; sleep current by profile; EMC/ESD and communication-loss behavior. Deliver EDA files and libraries, schematic PDF, BOM/MPNs and assembly profiles, Gerber/drill, assembly files, STEP, calculations, test firmware and reports.

## 13 Design inputs and component selection

Before fabrication, record valve models, pulse duration/current/polarity and cables; sensors and Modbus maps; 5 V/VBAT load currents; isolation and DC/DC per port; battery/charger and UVLO; VFD, feedback and safe-stop requirements; enclosure, connectors, temperature and EMC; regional plan and DX-LR30 revision. The contractor shall obtain customer approval for exact MPNs and calculated values. Port production firmware to S3-P2 separately from the working classic ESP32 prototype.

Pinout references: Espressif ESP32-S3-WROOM-1 datasheet v1.8 and Hardware Design Guidelines; TI TCA6424A Rev D and DRV8874. All 36 module GPIOs and 24 expander ports are listed in this specification.
