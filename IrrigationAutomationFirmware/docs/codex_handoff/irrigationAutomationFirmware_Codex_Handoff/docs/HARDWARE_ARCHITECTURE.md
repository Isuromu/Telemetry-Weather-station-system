# Hardware Architecture

## Prototype Rev A

```text
Solar panel
    |
Solar charge controller
    |
12 V lead-acid battery
    |
Over-discharge protection
    |
Protected 12 V system rail
    |
    +-----------------------> system DC/DC -> ESP32 / logic
    |
    +--> high-side P-MOSFET -> L298N power rail
                                 |
                                 +--> 12 V latching PCV solenoid

Battery voltage
    |
100 kOhm
    |
GPIO35
    |
22 kOhm
    |
GND
```

## PCV driver

Logical control:

- GPIO16 -> L298N IN1
- GPIO17 -> L298N IN2
- GPIO27 -> BJT -> P-channel MOSFET -> L298N switched power rail

Current active-level assumption:

- GPIO27 HIGH = L298N power ON
- GPIO27 LOW = L298N power OFF

Safe boot state:

- IN1 LOW
- IN2 LOW
- power OFF

## Pressure sensors

Prototype:

```text
ESP32 I2C controller 0
  SDA GPIO21
  SCL GPIO22
      |
      +--> UpstreamPressureSensor

ESP32 I2C controller 1
  SDA GPIO13
  SCL GPIO4
      |
      +--> DownstreamPressureSensor
```

External pull-ups are already fitted on the assembled prototype.

## Target Rev B

Preferred architecture:

```text
ESP32 I2C
SDA GPIO21
SCL GPIO22
    |
TCA9548A or equivalent I2C multiplexer
    |
    +--> channel 0 -> UpstreamPressureSensor
    |
    +--> channel 1 -> DownstreamPressureSensor
```

Benefits:

- identical fixed I2C addresses no longer require two ESP32 I2C pin pairs;
- GPIO13 and GPIO4 are released;
- cleaner scaling if more identical I2C devices are added.

## Future RS485 reservation

After the I2C multiplexer migration:

- GPIO13 -> UART TX
- GPIO34 -> UART RX
- GPIO4 -> optional DE/RE

For the auto-direction RS485 module, GPIO4 is not required.

## LoRa

```text
ESP32          SX1262 / DX-LR30
3V3         -> VCC
GPIO5       -> NSS
GPIO23      -> MOSI
GPIO19      -> MISO
GPIO18      -> SCK
GPIO26      -> DIO1
GPIO25      -> BUSY
GPIO14      -> NRST
GPIO33      -> RXEN
GPIO32      -> TXEN
GND         -> GND
```

## ESP32 pin-risk notes

- GPIO5 is a strapping pin and is currently used as LoRa NSS. Ensure the radio/module does not force an invalid level at reset.
- GPIO4 and GPIO13 are acceptable for the current second I2C bus.
- GPIO34–39 are input-only.
- GPIO6–11 are reserved by the module flash interface.
- Avoid casually consuming GPIO0/2/12/15 for new functions because they participate in boot strapping.

## Battery ADC notes

With 100 kOhm / 22 kOhm:

```text
ADC voltage = Battery voltage * 22 / 122
```

At 14.4 V battery voltage:

```text
ADC voltage ~= 2.60 V
```

This fits the configured attenuated ADC range.

Recommended hardware check for production:
- verify actual resistor values;
- consider a small capacitor at the ADC node for noise reduction;
- calibrate against a multimeter.
