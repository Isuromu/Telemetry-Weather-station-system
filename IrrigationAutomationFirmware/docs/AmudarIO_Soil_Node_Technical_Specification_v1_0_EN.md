# Soil node PCB technical specification

AmudarIO  |  Board 1  |  Version 1.0  |  4 September 2026  |  Pinout C6-P1

Design an autonomous PCB for one external 5 V RS-485 soil probe. The node measures soil temperature, volumetric water content, electrical conductivity and battery voltage, and sends the results to ChirpStack using LoRaWAN Class A.

## 1 Hardware and architecture

| Block | Implementation |
| --- | --- |
| MCU | ESP32-C6-MINI-1-H4, 4 MB flash |
| LoRa | SX1262-based DX-LR30-900M22S; external antenna through SMA |
| Battery | Removable protected 18650 Li-ion cell, 1S, 3.6/3.7 V nominal, 4.2 V charge |
| Holder | MPD BK-18650-PC2 with retainer; holder included in PCBA, cell supplied separately |
| Solar charging | LTC4079 with hardware NTC in physical contact with the cell |
| Always-on 3.3 V | TPS63900 buck-boost for ESP32 and LoRa |
| Switched 5 V probe rail | TPS61023 with EN and true output disconnect |
| RS-485 | THVD1406 automatic direction; power gated by TPS22917 |
| Battery measurement | Internal ADC; divider with TPS22917 high-side switch |
| Service | External USB-to-TTL on UART0; BOOT, RESET and SERVICE WAKE |

A nominal 6 V solar panel feeds the charger. The protected battery feeds the 3.3 V and 5 V converters through a mechanical SYSTEM switch. This switch disconnects all system loads; the charger and NTC remain connected to the panel and battery.

Do not fit GPS, an external RTC, ADS1115, a fuel gauge, continuously lit LEDs or an onboard USB-UART bridge. Disable Wi-Fi, BLE and 802.15.4 in deployed firmware. LiFePO4 is not an interchangeable battery option.

## 2 Operating cycle

Timer wake; safe initialization; enable probe and RS-485; wait for startup; read Modbus with bounded retries; disable the probe; measure battery voltage; send uplink; open RX1/RX2; process a command and acknowledge immediately; shut down LoRa; enter deep sleep. The server can change the interval, which must be retained. Do not execute a repeated command with the same ID.

## 3 Complete ESP32 C6 pinout

GPIO is the firmware identifier. Pad is the physical ESP32-C6-MINI-1-H4 module pad, not a bare-chip pin or DevKit header position. A = ADC input, I = input, O = output. All logic signals use 3.3 V levels.

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

Power: pad 3 = 3.3 V; pad 8 = EN/CHIP_PU. GND: pads 1, 2, 11, 14, 36-53. NC: pads 4, 7, 21, 32-35. GPIO10/11 are not exposed. No general-purpose GPIOs remain unallocated. Fit an EN RC network and supervisor suitable for slow and interrupted power ramps.

GPIO1 enables only the battery-divider switch. GPIO2 enables the 5 V boost and RS-485 power switch together. GPIOs must not carry load current or connect directly to the battery, 5 V or A/B lines.

## 4 GPIO restrictions and safe states

- GPIO4/5/8/9/15 are strapping pins. Do not load GPIO4/5. Pull GPIO8/9 to 3.3 V through 10 kOhm and GPIO15 to GND through 10 kOhm. UART download requires GPIO8=1 and GPIO9=0 at reset; normal boot requires GPIO9=1.
- GPIO6/7 reuse alternate JTAG functions. Do not enable external pad JTAG on GPIO4-7. Check the factory eFuse configuration; do not burn eFuses to accommodate the pinout.
- GPIO12/13 default to USB D-/D+. Disable USB functions and pulls, then assign UART1 before enabling RS-485. BOTH UART paths need powered-off isolation with specified Ioff. A series resistor alone is insufficient.
- Reserve GPIO16/17 for UART0. ROM TX logs must not control loads. GPIO18-23 are valid for LoRa, but require explicit SPI routing and consideration of startup SDIO pulls.

| Signals | Reset and startup | Deep sleep |
| --- | --- | --- |
| GPIO1/2 | LOW, external PD about 100 kOhm | LOW and held; divider and sensor domain OFF |
| GPIO3/7 RXEN/TXEN | LOW, PD about 100 kOhm | LOW and held |
| GPIO14/21 RESET/NSS | HIGH, PU 47-100 kOhm | HIGH after radio shutdown |
| GPIO18/20 SCK/MOSI | Keep NSS HIGH | LOW and held |
| GPIO6 WAKE | PU about 100 kOhm; button to GND | Wake input; HIGH when idle |
| ADC and other inputs | No overvoltage or back-power | Disable unused input buffers |

External circuitry must establish these states; firmware alone is insufficient. Before sleep, disconnect UART paths, set GPIO2/1 and RXEN/TXEN LOW, and configure hold. On wake, establish safe levels before releasing hold. Confirm pull values against leakage and noise margins.

## 5 LoRa connections

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

## 6 Power charging and battery measurement

Configure LTC4079 for 4.2 V and fixed input-voltage regulation based on the selected panel Vmp. It is a linear charger with Vmp regulation, not dynamic MPPT. Charge current must not exceed 250 mA or the cell, panel and thermal limits. Verify weak-light startup and charge termination with the node operating. Hardware NTC qualification must inhibit charging outside the cell temperature limits and on NTC faults.

If winter energy is insufficient, LTC4121-4.2 with fractional-Voc MPPT is an approval-required alternative after recalculating sleep current. Cell overcharge, over-discharge, overload and short-circuit protection are mandatory independently of the charger. Provide reverse-insertion protection, a cell retainer and an NTC contacting the cell sidewall.

Validate TPS63900 at minimum battery voltage and ESP32/LoRa peak load. The TPS61023 stage must disconnect and discharge its 5 V output; protect against shorts and reverse back-power. All disabled interfaces must remain high impedance.

Switch the battery divider on its high side. Starting values: 499 + 499 kOhm upper leg, 249 kOhm lower leg, 100 nF from ADC to GND. GPIO1 enables the switch; GPIO0 reads ADC1_CH0. Wait at least five RC time constants, discard the first sample, average readings and disable the switch. Confirm values for leakage, ADC loading and temperature; perform two-point calibration. Report voltage and normal/low/critical states, not an accurate state-of-charge percentage.

## 7 Connectors and RS485

Probe connector: 1 = +5V_SOIL_SW; 2 = GND/reference; 3 = RS485_A; 4 = RS485_B. Panel connector: PV+ and PV-. Use keyed, retained connectors with polarity labels. Select the connector family for the enclosure and cable.

THVD1406 direction is automatic; no MCU DE/RE is allocated. At the connector, fit an SM712-class TVS and footprints for surge-rated series resistors, optional 120 ohm termination and bias. Select values and termination population for the cable; power bias from the switched domain. Validate 4800/9600 baud, bus release timing and Modbus with the actual probe.

## 8 Programming and Serial Monitor

| Pin | Board signal | Adapter connection |
| --- | --- | --- |
| 1 | GND | GND |
| 2 | 3V3_REF | High-impedance reference input only; otherwise leave open |
| 3 | UART0_TX / GPIO16 | Adapter RX |
| 4 | UART0_RX / GPIO17 | Adapter TX |
| 5 | DTR | Through the two-transistor auto-reset circuit |
| 6 | RTS | Through the two-transistor auto-reset circuit |

Use 3.3 V logic only. Power the board from its own source; do not connect the adapter 3.3 V, 5 V or VBUS power outputs. BOOT: GPIO9; RESET: EN, module pad 8. Route DTR/RTS through the standard Espressif circuit, never directly to BOOT/EN. With a TX/RX/GND-only adapter, use the BOOT and RESET buttons. The same UART0 supports Serial Monitor; verify DTR/RTS behavior when opening the port.

## 9 PCB and power consumption requirements

Baseline: two-layer FR-4 with a nearly continuous ground plane. Four layers require a routing or EMC justification. ANT-to-SMA must be a short 50 ohm controlled-impedance trace calculated by the fabricator for the actual stackup. This is trace geometry, not a 50 ohm resistor. Keep power loops and inductors away from RF/ADC, respect the ESP32 antenna keepout, and locate protection at the connectors.

| Mode | Requirement |
| --- | --- |
| Normal timer deep sleep | Complete assembled-board current no greater than 25 uA |
| Inactive state and error backoff | Below 1 mA; probe, RS-485 and divider OFF |
| SYSTEM OFF | System loads unpowered; charging qualified by hardware NTC |
| Sensing, boot, LoRa TX/RX, programming, charging | Active modes; sleep-current limits do not apply |

Measure at the battery input with SYSTEM ON, panel and programmer disconnected, LEDs off, LoRa shut down and probe power removed. Use 3.70 V as the reference point; repeat at voltage and temperature limits. Also test with the unpowered probe connected. Include the cell protector, charger, supervisor, pulls and leakage in the budget. Provide a removable current-measurement link. Bound join/Modbus retries, then enter sleep.

## 10 Verification and contractor deliverables

- Verify power-up, BOOT/RESET, programming, watchdog and brownout without unintended probe power. Verify no back-power through UART or A/B.
- Test charging, NTC faults and temperature, 3.3 V under RF peaks, 5 V during probe startup and short circuit, ADC, Modbus, LoRaWAN join/uplink/RX1/RX2/ACK, sleep and wake.
- Deliver native EDA files and libraries, schematic PDF, BOM with exact manufacturer part numbers, Gerber/NC drill, assembly drawings and pick-and-place, STEP, ERC/DRC, power/RF/autonomy calculations, test firmware and measurement reports.

## 11 Design inputs and component selection

Before fabrication, record the protected 18650 model and dimensions, NTC and temperature limits; panel Voc/Vmp/Isc and power; probe model, Modbus map, current and startup time; DX-LR30 revision and regional plan; enclosure, outline, connectors and cables; winter autonomy, intervals, battery thresholds, rail/ADC tolerances and EMC/ESD levels. The contractor shall calculate component values and obtain customer approval.

Component references: Espressif ESP32-C6-MINI-1 datasheet and Hardware Design Guidelines; Analog Devices LTC4079; Texas Instruments TPS63900, TPS61023, TPS22917 and THVD1406. The pin-map identifier for this specification is C6-P1.
