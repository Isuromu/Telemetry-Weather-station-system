# Project Context — Irrigation Automation

## 1. System-level goal

The broader irrigation project is a distributed automation system combining field sensors, pressure control, a pump/VFD, LoRa communications and future RS485-controlled devices.

The current firmware focus is the pressure-control node, not the whole network at once.

Several related subsystems exist and must not be conflated:

1. **Pressure-control node**
   - ESP32-WROOM-32D / DevKitC V4.
   - two pressure sensors around a pressure-control/reducing valve;
   - 12 V latching solenoid;
   - L298N polarity-reversing driver;
   - LoRa radio;
   - battery monitoring;
   - future RS485.

2. **Pump/VFD subsystem**
   - electric water pump;
   - Delixi CDI-E series frequency inverter;
   - RS485/Modbus control planned/used from ESP32;
   - reverse is not used for the pump;
   - a minimum operating frequency around 10 Hz was discussed;
   - factory timeout/default behavior and safe run/stop behavior matter.

3. **Separate RS485 motorized valve actuator**
   - FC11C-related device/controller;
   - desired functions include open, close, position/percentage, feedback, calibration and other actuator features;
   - this is not the same device as the latching pressure-control valve.

4. **Soil sensor LoRa node**
   - ESP32 + LoRa + RS485 soil sensor;
   - intended sequence: wake -> self-check -> read sensor -> read battery -> transmit -> wait for ACK/config -> sleep;
   - gateway may return the next sample interval;
   - deep sleep is expected to use the ESP32 internal RTC unless later requirements justify an external RTC.

5. **Gateway**
   - Raspberry Pi / CM4-side system with RAK LoRa concentrator hardware;
   - ChirpStack, MQTT and related services have been used in the project.

## 2. Current pressure-control node power architecture

The earlier single-cell Li-ion + boost architecture is obsolete for this node.

Current architecture:

- 12 V lead-acid battery.
- solar panel charges the battery through a solar charge controller.
- an over-discharge protection circuit sits between the battery and the system.
- the ESP32/system power converters are fed from the protected battery rail.
- battery voltage is measured with an ADC divider on GPIO35.
- current divider in trainee firmware:
  - upper resistor: 100 kOhm;
  - lower resistor: 22 kOhm.
- the over-discharge circuit was previously designed around approximately:
  - cut-off: 11.8–11.9 V;
  - reconnect: about 12.6 V.
  These are prior design targets and should be checked against the actually assembled protection circuit before being treated as firmware thresholds.

## 3. Pressure-control architecture

The hydraulic device should be called `PressureControlValve`.

It is used as a pressure-control / pressure-reducing device.

Two pressure sensors are intentionally installed:

- `UpstreamPressureSensor` — before the pressure-control valve.
- `DownstreamPressureSensor` — after the pressure-control valve.

This enables:

- monitoring inlet pressure;
- monitoring regulated/downstream pressure;
- calculating pressure drop;
- detecting abnormal conditions;
- eventually verifying whether a commanded PCV action produced the expected hydraulic result.

The two sensors may have identical fixed I2C addresses.

Prototype Rev A therefore uses two independent I2C buses.

Target Rev B should use a single I2C bus with a multiplexer, freeing pins for RS485.

## 4. Latching solenoid

The newly received solenoids are confirmed to be latching.

Implications:

- the solenoid should not be powered continuously;
- OPEN and CLOSE use opposite polarities;
- each command is a pulse;
- after the pulse, the solenoid remains in the commanded magnetic/mechanical state without continuous current;
- the L298N is currently used to reverse polarity;
- the L298N power rail itself is switched by a P-channel MOSFET controlled through a BJT from GPIO27.

The old trainee `closeValve()` behavior, which merely removed power, is therefore incorrect.

## 5. High-side L298N power control

Current firmware assumption, matching the trainee comments:

- GPIO27 HIGH -> BJT turns on -> P-channel MOSFET gate is pulled low -> L298N 12 V rail turns on.
- GPIO27 LOW -> BJT turns off -> P-channel MOSFET gate returns high -> L298N power turns off.

This must be confirmed against the physical circuit if the schematic differs.

The firmware must always set L298N control inputs to a safe state before enabling the switched power rail.

## 6. LoRa hardware

Current fixed pinout:

- VCC: 3V3
- NSS: GPIO5
- MOSI: GPIO23
- DIO1: GPIO26
- RXEN: GPIO33
- NRST: GPIO14
- SCK: GPIO18
- MISO: GPIO19
- BUSY: GPIO25
- TXEN: GPIO32

The module previously used in this project is DX-LR30-900M22S / SX1262-class hardware.

GPIO5 is a classic ESP32 strapping pin. It can be used as NSS/CS, but the board must not force an invalid boot level.

## 7. Current ESP32 pinout

### Pressure-control node

- Battery ADC: GPIO35
- PCV L298N IN1: GPIO16
- PCV L298N IN2: GPIO17
- PCV L298N high-side power enable: GPIO27
- I2C bus 1 SDA: GPIO21
- I2C bus 1 SCL: GPIO22
- I2C bus 2 SDA: GPIO13
- I2C bus 2 SCL: GPIO4

GPIO4 and GPIO13 are not classic ESP32 boot strapping pins. They are acceptable for the prototype.

Classic ESP32 strapping pins include GPIO0, GPIO2, GPIO5, GPIO12 and GPIO15.

GPIO34–39 are input-only.

GPIO6–11 are associated with the module flash and should not be repurposed.

## 8. Future RS485 pin plan

With Prototype Rev A, GPIO13 and GPIO4 are occupied by the second I2C bus.

After moving the pressure sensors behind an I2C multiplexer:

- future RS485 TX: GPIO13
- future RS485 RX: GPIO34
- optional DE/RE: GPIO4

This is especially convenient because GPIO34 is input-only and therefore well suited for UART RX.

If the same auto-direction RS485 module used elsewhere in the project is retained, DE/RE is unnecessary and GPIO4 remains free.

ESP32 UART signals can be routed through the GPIO matrix; UART does not have to use the default GPIO16/GPIO17 pair.

## 9. XDB401 current working interpretation

Trainee code currently assumes:

- possible address 0x7F
- possible alternate address 0x6D
- pressure register 0x06
- temperature register 0x09
- measurement register 0x30
- start-measurement command 0x0A
- bit mask 0x08 used as a measurement-ready/busy condition
- pressure data: signed 24-bit big-endian
- temperature data: signed 16-bit big-endian
- sensor scale: 0–1 MPa = 0–10 bar
- pressure conversion:
  `rawPressure / 8388608.0 * 10.0 bar`
- temperature conversion:
  `rawTemperature / 256.0`

The I2C transaction structure itself is reasonable, but the register map and conversion formulas must be validated against the exact sensor documentation before production.

## 10. Battery ADC

Current trainee implementation:

- 12-bit ADC;
- 11 dB attenuation;
- 32 samples;
- `analogReadMilliVolts()`;
- divider 100 kOhm / 22 kOhm;
- calibration multiplier defaults to 1.0.

The firmware should preserve a calibration constant and compare ADC-derived battery voltage to a multimeter during commissioning.

## 11. LoRa transport architecture

Two example programs were requested:

1. Serial Monitor control.
2. LoRa control.

Both must call the same hardware-layer APIs.

Important unresolved point:

- the hardware radio is SX1262;
- the larger project also has a RAK concentrator + ChirpStack gateway;
- therefore the final network may be LoRaWAN;
- some earlier node behavior was described in application terms as direct send -> ACK -> receive sampling interval -> sleep.

Codex must inspect the existing repository and gateway firmware/configuration before choosing raw LoRa versus LoRaWAN. A raw RadioLib bring-up example is useful for radio hardware testing, but it must not silently become the final network protocol if the deployed network is LoRaWAN.

## 12. Existing RS485 assets

An internal RS485/Modbus transport library already exists in the user's file set:

- `RS485ModBus.h`
- `RS485ModBus.cpp`
- `PrintController.h`
- `PrintController.cpp`
- `library.json`

It supports:

- ESP32 HardwareSerial with custom RX/TX pins;
- optional DE direction control;
- configurable pre/post TX delays;
- RX buffering;
- Modbus CRC16 generation/verification;
- response window scanning;
- retries;
- debug logging.

Do not write a completely unrelated second Modbus transport implementation before checking whether this library should be reused/refactored.

## 13. VFD assets

The project has the Delixi manual:

`CDI-E frequency inverter manual.pdf`

Relevant sections include:

- control wiring;
- PID water-supply examples;
- Chapter 8 RS485 communication;
- Modbus command/status addresses.

Known manual examples include:

- command register A000H using function code 06H;
- 0001H = Forward Run;
- 0002H = Reverse Run;
- 0005H = Free Stop;
- 0006H = Shutdown by speed reduction;
- A001H = frequency command / upper frequency source;
- monitoring area starts around B000H.

The pump application does not need reverse operation.

## 14. Previous trainee firmware intent

The trainee code attempted to:

- initialize two I2C buses;
- find one pressure sensor on each;
- read pressure and temperature;
- read the battery;
- control the L298N through IN1/IN2;
- power the L298N through GPIO27;
- accept `open`, `close`, `status`, `help`;
- print status every two seconds.

Useful parts:
- two independent `TwoWire` instances;
- repeated-start I2C register read;
- received-length checking;
- sign extension of 24-bit pressure;
- averaged ADC reading.

Incorrect or weak parts:
- `closeValve()` did not reverse polarity and therefore did not close a latching solenoid;
- `valveIsOpen = false` at boot incorrectly claimed a physical state after reset;
- code mixed transport/UI, sensor logic, valve logic and application logic in one file;
- periodic full status every two seconds is useful for a test example but not the final low-power architecture;
- sensor conversion assumptions remain unverified;
- malformed pasted conditions lost `||` in two places.

## 15. Firmware design principle

Hardware functions must be transport-independent.

The following should be reusable from Serial, LoRa and later RS485/network logic:

- `BatteryMonitor`
- `PressureSensorXDB401`
- `PressureControlValve`
- `PressureControlSystem` or application service
- transport adapters: Serial / LoRa / future network

Do not create separate implementations of PCV actuation for Serial and LoRa.

## 16. Documentation language

Source code and internal code comments should be English.

A short Uzbek Latin quick reference may be maintained for field/service documentation.
