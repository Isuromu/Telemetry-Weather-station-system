# ESP32 DevKitC V4 pinout

This file records the live classic-ESP32 prototype assignments. It does not
define the future ESP32-S3 production controller. The provisional two-valve
production map is in
`UNIVERSAL_12V_CONTROLLER_PCB_TECHNICAL_SPEC_DRAFT.md`, Section 14.

Production-board review files (GPIO plus physical module pad):

- [Soil ESP32-C6, C6-P1](SOIL_NODE_ESP32C6_PINOUT_RU.md)
- [Universal ESP32-S3, S3-P2](UNIVERSAL_12V_ESP32S3_PINOUT_RU.md)

S3-P2 supersedes the old production draft and reserves GPIO39-42 for JTAG.
None of the prototype connections below were changed.

## Active pump/VFD connections

| Function | ESP32 GPIO | Notes |
|---|---:|---|
| Debug Serial | USB UART0 | 115200 baud |
| RS-485 RXD | 16 | UART2 RX from converter |
| RS-485 TXD | 17 | UART2 TX to converter |
| Device power control | 2 | Reserved; unused by the pump example |

GPIO2 replaces the older GPIO34 assignment on the supplied hand sketch.
GPIO34 is input-only and is not used for transistor control.

## LoRa allocation

The pressure/valve target uses this allocation for the LR-DX30/SX1262 module.
The pump target still treats it as reserved.

| LoRa signal | ESP32 GPIO |
|---|---:|
| MOSI | 23 |
| MISO | 19 |
| SCK | 18 |
| NSS | 5 |
| NRST | 14 |
| BUSY | 25 |
| DIO1 | 26 |
| DIO2 | Not connected |
| RXEN | 33 |
| TXEN | 32 |

## ESP32-H2

`config/boards/ESP32_H2.h` intentionally contains no GPIO assignments. Do not
select it until the user-provided H2 pinout has been added and reviewed.

## Pressure-control node Prototype Rev A

This is a separate build target from the pump/VFD node.

| Function | ESP32 GPIO | Notes |
|---|---:|---|
| Battery ADC | 35 | 100 kOhm / likely 20 kOhm divider; 100 nF at ADC input |
| PCV L298N IN1 | 2 | Strapping pin; installed 20 kOhm pull-down |
| PCV L298N IN2 | 15 | Strapping pin; installed 20 kOhm pull-down |
| PCV L298N power enable | 27 | Assumed active HIGH; fit pull-down and verify hardware |
| Upstream I2C SDA/SCL | 21 / 22 | XDB401 Prototype Rev A |
| Downstream I2C SDA/SCL | 13 / 4 | XDB401 Prototype Rev A |
| TUF-2000M RS-485 RX/TX | 16 / 17 | UART2; auto-direction converter |

GPIO2 and GPIO15 are ESP32-WROOM boot-strapping pins. GPIO2 defaults low;
GPIO15 defaults high and controls boot-time U0TXD output. The L298N rail must
remain physically disabled throughout reset, and both L298N inputs must have
20 kOhm pull-downs. The GPIO27 power-control circuit must have its own hardware
pull-down at the ESP32/transistor control input so firmware is not the only
protection against an unintended pulse.

Do not use GPIO0 or GPIO12 as substitutes: GPIO0 directly selects download
mode and GPIO12 can select the flash-supply voltage at reset. ESP32 latches
strapping levels during reset and releases the pins for GPIO use afterward.

The automatic-direction RS-485 module requires no DE/RE pin. Connect ESP32
GPIO16 (RX) to the converter TX/RO side and GPIO17 (TX) to its RX/DI side;
connect A/B according to the actual converter labels and verify polarity on
the bench.
