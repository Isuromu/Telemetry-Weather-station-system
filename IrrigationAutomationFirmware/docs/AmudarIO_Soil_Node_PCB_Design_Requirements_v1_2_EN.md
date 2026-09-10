# Soil node PCB design requirements

AmudarIO  |  Board 1  |  Version 1.2  |  4 September 2026  |  Pinout C6-P2

Scope: electrical schematic capture and PCB design for the hardware specified below. Deliver design files only. Firmware, command protocols, cloud services, enclosure design, manufacturing, assembly, commissioning and physical product testing are outside this assignment. Board outline, mounting coordinates and external interface requirements are supplied by the customer.

## 1 Hardware composition

| Block | Required implementation |
| --- | --- |
| MCU | ESP32-C6-MINI-1-H4, 4 MB flash; pin map C6-P2 |
| Radio | SX1262-based DX-LR30-900M22S, 3.3 V; external LoRa antenna via SMA |
| Battery | Removable protected 1S 18650 Li-ion cell, 3.6/3.7 V nominal, 4.2 V charge |
| Holder | MPD BK-18650-PC2 with retainer; PCB includes holder, not the cell |
| Solar MPPT charger | LTC4121IUD-4.2#PBF; autonomous fractional-Voc MPPT, 4.2 V CC/CV |
| GPS and GNSS | u-blox MAX-M10S-00B; switched 3.3 V supply and separate GNSS antenna |
| UART selection | TMUX1574PW; power gated, soil/GNSS selector on UART1 |
| System 3.3 V | TPS63900 buck-boost, always on when SYSTEM is ON |
| Probe 5 V | TPS61023 with EN and true output disconnect |
| RS-485 | THVD1406 automatic direction; 3.3 V rail switched by TPS22917 |
| Battery ADC | Internal ADC; divider switched on its high side by TPS22917 |
| Service | UART0 header, DTR/RTS circuit, BOOT and RESET; no separate wake button |

MPPT and GPS/GNSS are mandatory fitted functions. Do not fit an external RTC, ADS1115, a fuel gauge or continuously lit LEDs. LiFePO4 is not an interchangeable cell option.

## 2 Power architecture and MPPT charger

A nominal 6 V panel feeds the charger. The protected battery feeds both the 3.3 V converter and 5 V boost through the mechanical SYSTEM switch. SYSTEM OFF disconnects all system loads; the panel, charger, battery and NTC remain connected. Prevent back-power through service and probe connectors.

Use LTC4121-4.2 with its MPPT divider fitted and enabled. It periodically samples panel open-circuit voltage and regulates a programmed fraction of it; this is autonomous fractional-Voc MPPT, not fixed-Vmp regulation or a full power-curve sweep. MPPT must operate with the MCU asleep and SYSTEM OFF. A non-MPPT substitute is not permitted. [1]

Select the divider ratio from the actual panel Vmp/Voc; do not assume a universal percentage. The charger needs at least 4.4 V and battery headroom after the input blocking element. Check hot-panel Vmp, cold-panel Voc, input capacitance and weak-light startup. Program 50-250 mA within cell, panel and thermal limits. Follow the solar reference circuit: place MPPT/RUN sensing on the panel side of reverse blocking to prevent dark battery drain. [1]

Fit a cell-contact NTC with hardware charge inhibition for unsafe temperature and NTC faults. Independent cell protection covers overvoltage, undervoltage, overload and short circuit; add reverse-insertion protection. Size converter peaks and capacitance from the supplied probe, radio and GNSS loads. The charger is not an MCU-controlled power switch.

## 3 Complete ESP32 C6 pinout

GPIO is the signal identifier; Pad is the physical ESP32-C6-MINI-1-H4 module pad, not a bare-chip pin or DevKit header. A = ADC input; I = input; O = output. Logic level: 3.3 V.

| GPIO | Pad | I/O | Net | Function |
| --- | --- | --- | --- | --- |
| 0 | 12 | A | VBAT_ADC | Battery voltage, ADC1_CH0 |
| 1 | 13 | O | VBAT_DIV_EN | Battery divider high-side switch enable |
| 2 | 5 | O | PERIPH_PWR_EN | Enable selected soil or GNSS domain and UART mux |
| 3 | 6 | O | LORA_RXEN | LoRa RF switch receive control |
| 4 | 9 | - | RESERVED_STRAP_MTMS | Reserved SDIO strap / MTMS |
| 5 | 10 | - | RESERVED_STRAP_MTDI | Reserved SDIO strap / MTDI |
| 6 | 15 | O | GNSS_SEL | 0 = soil; 1 = GNSS; gated by GPIO2 |
| 7 | 16 | O | LORA_TXEN | LoRa RF switch transmit control |
| 8 | 22 | - | RESERVED_BOOT_STRAP | Reserved boot strap |
| 9 | 23 | I | BOOT_N | BOOT button, programming and recovery only |
| 12 | 17 | O | PERIPH_UART_TX | UART1 TX to soil or GNSS through mux |
| 13 | 18 | I | PERIPH_UART_RX | UART1 RX from selected soil or GNSS branch |
| 14 | 19 | O | LORA_RESET_N | LoRa reset, active LOW |
| 15 | 20 | - | RESERVED_JTAG_STRAP | Reserved JTAG source-selection strap |
| 16 | 31 | O | UART0_TX | UART0 TX, logs and external USB-to-TTL |
| 17 | 30 | I | UART0_RX | UART0 RX, programming and Serial commands |
| 18 | 24 | O | LORA_SCK | LoRa SPI clock |
| 19 | 25 | I | LORA_MISO | SPI data from LoRa to MCU |
| 20 | 26 | O | LORA_MOSI | SPI data from MCU to LoRa |
| 21 | 27 | O | LORA_NSS | LoRa SPI chip select, active LOW |
| 22 | 28 | I | LORA_DIO1 | LoRa interrupt |
| 23 | 29 | I | LORA_BUSY | LoRa busy status |

Supply: pad 3 = 3.3 V; pad 8 = EN/CHIP_PU. GND: pads 1, 2, 11, 14, 36-53. NC: pads 4, 7, 21, 32-35. GPIO10/11 are not exposed. No spare GPIOs remain. Add EN pull-up, RC and supervisor for slow/interrupted supply ramps.

C6-P2 changes GPIO6 from SERVICE_WAKE_N to GNSS_SEL and routes UART1 through a selector. GPIO2 is now PERIPH_PWR_EN; it powers only the branch selected by GPIO6. GPIO1 still independently enables the battery divider. LoRa and UART0 pin numbers are unchanged. No load current flows through GPIOs.

Do not connect GPIOs directly to battery voltage, 5 V or RS-485 A/B. The extra wake button is omitted; BOOT and RESET remain. No strapping or flash pin is repurposed for GNSS.

## 4 Pin restrictions and electrical defaults

- GPIO4/5/8/9/15 are strapping pins. Do not load GPIO4/5. Pull GPIO8/9 HIGH through 10 kOhm and GPIO15 LOW through 10 kOhm. BOOT pulls GPIO9 to GND; preserve GPIO8=1 and GPIO9=0 for UART download, GPIO9=1 for normal boot.
- GPIO6/7 reuse pad-JTAG functions. Do not connect an external JTAG interface to GPIO4-7 in this design. Respect the factory eFuse/JTAG source configuration; no eFuse changes are part of PCB design.
- GPIO12/13 default to USB D-/D+. Both UART paths require powered-off isolation from soil AND GNSS, including startup. The mux must be disconnected while either selected rail is invalid. A series resistor alone does not establish isolation.
- GPIO16/17 are dedicated UART0. GPIO18-23 can have startup SDIO pulls. Keep LoRa NSS inactive during startup; do not attach power enables to UART0.

| Nets | External reset defaults | Required low-power levels |
| --- | --- | --- |
| GPIO1/2 | PD about 100 kOhm; LOW | LOW; divider, soil, GNSS and mux OFF |
| GPIO3/7 RXEN/TXEN | PD about 100 kOhm; LOW | LOW |
| GPIO14/21 RESET/NSS | PU 47-100 kOhm; HIGH | HIGH with radio in shutdown |
| GPIO18/20 SCK/MOSI | Keep NSS HIGH | LOW |
| GPIO6 GNSS_SEL | PD 10 kOhm; GPIO2 blocks all loads | LOW; soil selected but unpowered |
| ADC and UART inputs | Voltage limits and no back-power | No floating or powered-off leakage paths |

GPIO6 has a reset-time internal weak pull-up. Size its external pull-down for margin; GPIO2 LOW must independently inhibit both domains regardless of GPIO6. All loads remain off while MCU pins are high impedance. Low-power levels are electrical interface requirements, not a software work item.

## 5 Radio wiring

| Signal | DX-LR30 pin | ESP32 GPIO |
| --- | --- | --- |
| RXEN | 6 | 3 |
| TXEN | 7 | 7 |
| DIO1 | 13 | 22 |
| BUSY | 14 | 23 |
| RESET | 15 | 14 |
| MISO | 16 | 19 |
| MOSI | 17 | 20 |
| SCK | 18 | 18 |
| NSS | 19 | 21 |

DX-LR30 supply: 3.3 V, pin 9 in the project reference. Leave DIO2/DIO3 unconnected. Verify the purchased module marking and revision drawing before assigning its footprint. LoRa module pin numbers are not ESP32 GPIO numbers.

## 6 GPS and GNSS hardware

Fit MAX-M10S for autonomous outdoor position acquisition, including GPS. Coordinates must be electrically accessible to the ESP32 through UART1 for subsequent transmission by the existing radio. No location-update interval or application protocol is assigned to the PCB designer. GNSS and soil sensing are sequential, not simultaneous. [2]

| MAX-M10S pins | Connection |
| --- | --- |
| 1, 10, 12 / GND | Ground plane |
| 7 / V_IO; 8 / VCC | 3V3_GNSS_SW from a TPS22917 load switch |
| 2 / TXD | Through UART mux to ESP32 GPIO13 |
| 3 / RXD | Through UART mux from ESP32 GPIO12 |
| 6 / V_BCKP | Leave open; no backup cell or always-on backup supply |
| 11 / RF_IN | 50 ohm path to a separate GNSS U.FL antenna connector |
| 15 / VIO_SEL | Leave open for 3.3 V I/O |
| 9 / RESET_N | Local reset test pad; no pull-up to an always-on rail |
| 4, 5, 13, 14, 16, 17, 18 | Unused in this UART and external antenna-bias implementation; leave open |

Switch VCC and V_IO together; remove antenna bias at the same time. Allow at least 100 mA receiver startup current plus the selected antenna load and margin. Respect the V_IO ramp limits and add local decoupling. Cold starts after full shutdown are expected. No GNSS pin may back-power the switched supply. [2, 3]

Use an external active L1 GNSS antenna rated for 3.3 V through a short coax lead. Feed bias from 3V3_GNSS_SW through a current-limited, filtered bias tee and add low-capacitance RF ESD protection. Select gain, current and bias components per the integration guide. Keep GNSS away from LoRa TX and switching nodes; do not share the LoRa SMA. The antenna needs sky visibility. [3]

## 7 Power and UART selection

| GPIO2 | GPIO6 | Powered domain | UART1 connection |
| --- | --- | --- | --- |
| 0 | X | Both OFF; mux supply OFF | Disconnected |
| 1 | 0 | Soil 5 V and RS-485 3.3 V only | Soil RS-485 |
| 1 | 1 | GNSS and antenna only | GNSS |

Hardware decode: SOIL_EN = GPIO2 AND NOT GPIO6; GNSS_EN = GPIO2 AND GPIO6. Apply reset/brownout inhibition. Power the TMUX1574 from a third switched 3.3 V rail enabled by GPIO2 so its standby current is absent in deep sleep. UART0 is never multiplexed.

TMUX1574PW TSSOP: D1 pin 4 to GPIO12; S1A pin 2 to RS-485 DI; S1B pin 3 to GNSS RXD. D2 pin 7 to GPIO13; S2A pin 5 to RS-485 RO; S2B pin 6 to GNSS TXD. SEL pin 1 to GPIO6; VDD pin 16 to switched mux supply; GND pin 8 to GND. Ground unused channels at pins 9-14. [4]

EN pin 15 is active LOW. Hardware must hold it HIGH until the selected supply is valid, and disconnect it before power removal or branch changes. Use break-before-make gating and specified off-state isolation; the mux alone does not protect an unpowered endpoint while its channel is ON. Both domains must be off during selection changes. Add test pads for both UART branches and supply enables. [4]

## 8 Switched probe rail and battery ADC

SOIL_EN enables TPS61023 and the RS-485 supply switch together. Add output discharge and probe short-circuit protection. A/B and both UART paths must not back-power disabled rails. Use a keyed probe connector: 1 = +5V_SOIL_SW, 2 = GND/reference, 3 = RS485_A, 4 = RS485_B. Panel connector: PV+ and PV-.

RS-485 direction is automatic; no DE/RE GPIO is allocated. Provide connector-side SM712-class TVS, surge-rated series-resistor footprints and optional 120 ohm termination/bias. Bias belongs to the switched domain. Select termination and AutoDirection timing provisions for the supplied cable and baud rate, including 4800/9600 baud requirements.

Battery divider starting values: high-side switch, 499 + 499 kOhm upper leg, 249 kOhm lower leg and 100 nF ADC-to-GND. GPIO1 controls the switch; GPIO0 is ADC1_CH0. Check divider ratio, RC settling, resistor tolerances, leakage and ADC voltage limits. Include a voltage test pad; no calibration code is required.

## 9 Service connector

| Pin | Board net | Connection |
| --- | --- | --- |
| 1 | GND | Common ground |
| 2 | 3V3_REF | High-impedance adapter reference only; not a power input |
| 3 | UART0_TX / GPIO16 | External adapter RX |
| 4 | UART0_RX / GPIO17 | External adapter TX |
| 5 | DTR | Input to onboard two-transistor auto-reset circuit |
| 6 | RTS | Input to onboard two-transistor auto-reset circuit |

External USB-to-TTL interface: 3.3 V logic only. The board uses its own power source. Do not connect adapter 3.3 V, 5 V or VBUS power outputs to the board. Route DTR/RTS through the standard Espressif two-transistor circuit, not directly to the MCU. Provide independent BOOT (GPIO9) and RESET (EN, module pad 8) buttons or pads. No onboard USB-UART bridge is required.

## 10 PCB layout and design outputs

Use two-layer FR-4 with a nearly continuous GND plane. Four layers require routing/EMC justification. Route ANT-to-SMA as a short 50 ohm transmission line using fabricator-confirmed geometry, not a series 50 ohm resistor. Keep switching loops/inductors away from ADC/RF, respect module antenna keepouts and place protection beside connectors.

Sleep target: complete board <=25 uA; mandatory inactive-state ceiling <1 mA. Conditions: timer deep sleep, radio shutdown, soil/GNSS/antenna/divider/mux OFF, adapter detached and dark panel. Acquisition, sensing, radio activity and charging are active states. Budget all leakage across temperature, including the MPPT charger and isolation; <=25 uA is not yet a validated worst-case result. Do not delete MPPT or GNSS to meet the target. Fit a current-measurement link; physical testing is outside this assignment.

Deliver the native schematic/PCB project with libraries; schematic PDF; BOM with exact manufacturer part numbers and DNP variants; Gerber and NC drill files; fabrication notes and stackup; pick-and-place and assembly drawings; PCB-only STEP model; ERC/DRC results with justified exceptions; RF impedance geometry and electrical sizing notes. Assembly files are design outputs, not an obligation to manufacture or assemble boards.

Before design release confirm cell/NTC, panel Voc/Vmp/Isc, probe load, radio revision/band, GNSS antenna, board outline/connector locations and environmental ratings. Do not invent missing load ratings.

## 11 Component design references

[1] ADI LTC4121-4.2: analog.com/media/en/technical-documentation/data-sheets/4121fbc.pdf

[2] u-blox MAX-M10S: content.u-blox.com/sites/default/files/MAX-M10S_DataSheet_UBX-20035208.pdf

[3] MAX-M10S integration: content.u-blox.com/sites/default/files/MAX-M10S_IntegrationManual_UBX-20053088.pdf

[4] TI TMUX1574: ti.com/lit/ds/symlink/tmux1574.pdf
