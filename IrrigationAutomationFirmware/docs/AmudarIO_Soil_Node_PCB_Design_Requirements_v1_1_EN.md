# Soil node PCB design requirements

AmudarIO  |  Board 1  |  Version 1.1  |  4 September 2026  |  Pinout C6-P1

Scope: electrical schematic capture and PCB design for the hardware specified below. Deliver design files only. Firmware, command protocols, cloud services, enclosure design, manufacturing, assembly, commissioning and physical product testing are outside this assignment. Board outline, mounting coordinates and external interface requirements are supplied by the customer.

## 1 Hardware composition

| Block | Required implementation |
| --- | --- |
| MCU | ESP32-C6-MINI-1-H4, 4 MB flash; pin map C6-P1 |
| Radio | SX1262-based DX-LR30-900M22S, 3.3 V; external LoRa antenna via SMA |
| Battery | Removable protected 1S 18650 Li-ion cell, 3.6/3.7 V nominal, 4.2 V charge |
| Holder | MPD BK-18650-PC2 with retainer; PCB includes holder, not the cell |
| Solar charger | LTC4079; hardware NTC contacting the battery sidewall |
| System 3.3 V | TPS63900 buck-boost, always on when SYSTEM is ON |
| Probe 5 V | TPS61023 with EN and true output disconnect |
| RS-485 | THVD1406 automatic direction; 3.3 V rail switched by TPS22917 |
| Battery ADC | Internal ADC; divider switched on its high side by TPS22917 |
| Service | UART0 header, DTR/RTS circuit, BOOT, RESET and SERVICE WAKE |

Do not fit GPS, an external RTC, ADS1115, a fuel gauge, continuously lit LEDs or additional radio interfaces. LiFePO4 is not an interchangeable cell option.

## 2 Power architecture and charger

A nominal 6 V panel feeds the charger. The protected battery feeds both the 3.3 V converter and 5 V boost through the mechanical SYSTEM switch. SYSTEM OFF disconnects all system loads; the panel, charger, battery and NTC remain connected. Prevent back-power through service and probe connectors.

Configure LTC4079 for 4.2 V and input-voltage regulation based on the panel Vmp. This is fixed-Vmp linear charging, not dynamic MPPT. Set charge current no higher than 250 mA or the panel, cell and thermal limits. Provide hardware charge inhibition at the approved cell temperature limits and on NTC faults. Cell protection must independently cover overvoltage, undervoltage, overload and short circuit; provide reverse-insertion protection.

Calculate converter peak-current margin, charger dissipation and rail capacitance from the customer-supplied loads and panel data. Any charger substitution requires customer approval; do not change the circuit merely to provide a different MPPT method.

## 3 Complete ESP32 C6 pinout

GPIO is the signal identifier; Pad is the physical ESP32-C6-MINI-1-H4 module pad, not a bare-chip pin or DevKit header. A = ADC input; I = input; O = output. Logic level: 3.3 V.

| GPIO | Pad | I/O | Net | Function |
| --- | --- | --- | --- | --- |
| 0 | 12 | A | VBAT_ADC | Battery voltage, ADC1_CH0 |
| 1 | 13 | O | VBAT_DIV_EN | Battery divider high-side switch enable |
| 2 | 5 | O | SENSOR_DOMAIN_EN | Shared enable for 5 V boost and RS-485 power |
| 3 | 6 | O | LORA_RXEN | LoRa RF switch receive control |
| 4 | 9 | - | RESERVED_STRAP_MTMS | Reserved SDIO strap / MTMS |
| 5 | 10 | - | RESERVED_STRAP_MTDI | Reserved SDIO strap / MTDI |
| 6 | 15 | I | SERVICE_WAKE_N | Wake button, connects to GND |
| 7 | 16 | O | LORA_TXEN | LoRa RF switch transmit control |
| 8 | 22 | - | RESERVED_BOOT_STRAP | Reserved boot strap |
| 9 | 23 | I | BOOT_N | BOOT button, programming and recovery only |
| 12 | 17 | O | RS485_TX | UART1 TX to automatic-direction RS-485 |
| 13 | 18 | I | RS485_RX | UART1 RX from automatic-direction RS-485 |
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

GPIO1 drives the divider-switch enable; GPIO2 drives both the 5 V boost EN and RS-485 power-switch enable. These GPIOs do not carry load current. Do not connect GPIOs directly to battery voltage, 5 V or RS-485 A/B.

## 4 Pin restrictions and electrical defaults

- GPIO4/5/8/9/15 are strapping pins. Do not load GPIO4/5. Pull GPIO8/9 HIGH through 10 kOhm and GPIO15 LOW through 10 kOhm. BOOT pulls GPIO9 to GND; preserve GPIO8=1 and GPIO9=0 for UART download, GPIO9=1 for normal boot.
- GPIO6/7 reuse pad-JTAG functions. Do not connect an external JTAG interface to GPIO4-7 in this design. Respect the factory eFuse/JTAG source configuration; no eFuse changes are part of PCB design.
- GPIO12/13 default to USB D-/D+. Both UART connections to the switched RS-485 domain require powered-off isolation with specified Ioff, including during MCU startup. A series resistor alone does not establish isolation.
- GPIO16/17 are dedicated UART0. GPIO18-23 can have startup SDIO pulls. Keep LoRa NSS inactive during startup; do not attach power enables to UART0.

| Nets | External reset defaults | Required low-power levels |
| --- | --- | --- |
| GPIO1/2 | PD about 100 kOhm; LOW | LOW; divider and probe domain OFF |
| GPIO3/7 RXEN/TXEN | PD about 100 kOhm; LOW | LOW |
| GPIO14/21 RESET/NSS | PU 47-100 kOhm; HIGH | HIGH with radio in shutdown |
| GPIO18/20 SCK/MOSI | Keep NSS HIGH | LOW |
| GPIO6 SERVICE_WAKE_N | PU about 100 kOhm; button to GND | HIGH when released |
| ADC and UART inputs | Voltage limits and no back-power | No floating or powered-off leakage paths |

The pull network must keep external power domains safe while MCU pins are high impedance. Pull values are starting values; check logic margin and leakage. Low-power states are electrical interface conditions, not a software work item.

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

## 6 Switched probe rail and battery ADC

GPIO2 HIGH enables TPS61023 and the RS-485 supply switch together; LOW disables both. Add output discharge and probe short-circuit protection. A/B and both UART paths must not back-power disabled rails. Use a keyed probe connector: 1 = +5V_SOIL_SW, 2 = GND/reference, 3 = RS485_A, 4 = RS485_B. Panel connector: PV+ and PV-.

RS-485 direction is automatic; no DE/RE GPIO is allocated. Provide connector-side SM712-class TVS, surge-rated series-resistor footprints and optional 120 ohm termination/bias. Bias belongs to the switched domain. Select termination and AutoDirection timing provisions for the supplied cable and baud rate, including 4800/9600 baud requirements.

Battery divider starting values: high-side switch, 499 + 499 kOhm upper leg, 249 kOhm lower leg and 100 nF ADC-to-GND. GPIO1 controls the switch; GPIO0 is ADC1_CH0. Check divider ratio, RC settling, resistor tolerances, leakage and ADC voltage limits. Include a voltage test pad; no calibration code is required.

## 7 Service connector

| Pin | Board net | Connection |
| --- | --- | --- |
| 1 | GND | Common ground |
| 2 | 3V3_REF | High-impedance adapter reference only; not a power input |
| 3 | UART0_TX / GPIO16 | External adapter RX |
| 4 | UART0_RX / GPIO17 | External adapter TX |
| 5 | DTR | Input to onboard two-transistor auto-reset circuit |
| 6 | RTS | Input to onboard two-transistor auto-reset circuit |

External USB-to-TTL interface: 3.3 V logic only. The board uses its own power source. Do not connect adapter 3.3 V, 5 V or VBUS power outputs to the board. Route DTR/RTS through the standard Espressif two-transistor circuit, not directly to the MCU. Provide independent BOOT (GPIO9) and RESET (EN, module pad 8) buttons or pads. No onboard USB-UART bridge is required.

## 8 PCB layout and design outputs

Use two-layer FR-4 with a nearly continuous GND plane. Four layers require routing/EMC justification. Route ANT-to-SMA as a short 50 ohm transmission line using fabricator-confirmed geometry, not a series 50 ohm resistor. Keep switching loops/inductors away from ADC/RF, respect module antenna keepouts and place protection beside connectors.

Low-power design limits: complete board <=25 uA with MCU in timer deep sleep, radio in shutdown, probe/RS-485/divider OFF, and charger input absent; inactive states <1 mA. Active loads and charging are excluded. Budget maximum leakage including charger, protector, supervisor, switches and pulls. Include a removable current-measurement link. Physical current testing is performed outside this PCB-design assignment.

Deliver the native schematic/PCB project with libraries; schematic PDF; BOM with exact manufacturer part numbers and DNP variants; Gerber and NC drill files; fabrication notes and stackup; pick-and-place and assembly drawings; PCB-only STEP model; ERC/DRC results with justified exceptions; RF impedance geometry and electrical sizing notes. Assembly files are design outputs, not an obligation to manufacture or assemble boards.

Customer inputs before design release: exact cell/NTC and panel ratings; probe supply/current/cable data; radio revision and RF band; board outline, holes and connector locations; temperature and surge requirements. Do not invent missing load ratings. Use current manufacturer footprints and electrical design guidance.
