# Soil node ESP32 C6 pinout

Revision C6-P2, 2026-09-04. Exact module: ESP32-C6-MINI-1-H4.
Companion specification: `AmudarIO_Soil_Node_PCB_Design_Requirements_v1_2_EN.md`.
This supersedes C6-P1 for production PCB design, not assembled prototype wiring.

## GPIO and module pad allocation

GPIO numbers are logical signals; pad numbers refer to the MINI module.
All interfaces use 3.3 V logic. No battery, 5 V or RS485 A/B connects to a GPIO.

| GPIO | Module pad | Type | Net | Function |
|---|---|---|---|---|
| 0 | 12 | A | VBAT_ADC | Battery ADC1_CH0 |
| 1 | 13 | O | VBAT_DIV_EN | Battery-divider high-side enable |
| 2 | 5 | O | PERIPH_PWR_EN | Master permission for selected soil/GNSS domain and UART mux |
| 3 | 6 | O | LORA_RXEN | LoRa receive RF switch |
| 4 | 9 | - | RESERVED_STRAP_MTMS | SDIO strap / JTAG MTMS; no peripheral |
| 5 | 10 | - | RESERVED_STRAP_MTDI | SDIO strap / JTAG MTDI; no peripheral |
| 6 | 15 | O | GNSS_SEL | LOW selects soil; HIGH selects GNSS; no wake button |
| 7 | 16 | O | LORA_TXEN | LoRa transmit RF switch |
| 8 | 22 | - | RESERVED_BOOT_STRAP | Boot strap, pull HIGH |
| 9 | 23 | I | BOOT_N | BOOT button, pull HIGH |
| 12 | 17 | O | PERIPH_UART_TX | UART1 TX to selector |
| 13 | 18 | I | PERIPH_UART_RX | UART1 RX from selector |
| 14 | 19 | O | LORA_RESET_N | LoRa reset, active LOW |
| 15 | 20 | - | RESERVED_JTAG_STRAP | JTAG source strap, pull LOW |
| 16 | 31 | O | UART0_TX | Dedicated external service adapter RX |
| 17 | 30 | I | UART0_RX | Dedicated external service adapter TX |
| 18 | 24 | O | LORA_SCK | LoRa SPI clock |
| 19 | 25 | I | LORA_MISO | LoRa SPI data to MCU |
| 20 | 26 | O | LORA_MOSI | LoRa SPI data from MCU |
| 21 | 27 | O | LORA_NSS | LoRa SPI select, active LOW |
| 22 | 28 | I | LORA_DIO1 | LoRa interrupt |
| 23 | 29 | I | LORA_BUSY | LoRa busy status |

Supply: pad 3 = 3.3 V; pad 8 = EN/CHIP_PU. Ground pads 1, 2, 11, 14, 36-53.
NC pads 4, 7, 21, 32-35. GPIO10/11 are not exposed. Internal flash wiring is
not available. Preserve all strap restrictions and LoRa module cross-references
in the companion specification; no GNSS signal uses a strap or flash pin.

## Power selection

| GPIO2 | GPIO6 | Power and UART |
|---|---|---|
| 0 | X | Soil, GNSS, antenna and mux supply OFF; UART paths isolated |
| 1 | 0 | Soil boost and RS485 ON; UART1 to soil |
| 1 | 1 | GNSS and antenna ON; UART1 to GNSS |

Hardware decode: SOIL_EN = GPIO2 AND NOT GPIO6; GNSS_EN = GPIO2 AND GPIO6.
GPIO1 remains independent. Reset/brownout inhibition overrides these requests.
GPIO6 has an internal reset pull-up; the master GPIO2 default LOW must keep all
loads OFF regardless of selector state. UART0/BOOT/RESET remain independent.

Use power-gated TMUX1574PW, with its exact package-pin wiring and active-LOW
EN qualification specified in the companion document. The mux and selected
endpoint must not back-power one another during power ramps or selection.
Apply break-before-make blanking and rail-valid gating. GNSS/soil UART use is
sequential. MAX-M10S V_BCKP is open and antenna bias is switched; no always-on
GNSS backup domain is fitted.
