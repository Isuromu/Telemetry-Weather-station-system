# AmudarIO universal 12 V controller PCB technical specification - review draft

## 1. Document status

Document revision: `Draft 0.2`

Updated: 2026-09-04. Full GPIO/module-pad and expander tables are in Section 14.
Russian standalone review copy:
[UNIVERSAL_12V_ESP32S3_PINOUT_RU.md](UNIVERSAL_12V_ESP32S3_PINOUT_RU.md).

Purpose: architecture review and owner approval before schematic capture for
the second production PCB family.

This document is not an approved schematic, released BOM, purchasing list or
manufacturing package. Parts marked `PROPOSED` are engineering recommendations.
Values marked `TBD` must not be invented by the PCB contractor.

The source HRS and archived project handoff are technical reference material.
They do not override direct decisions made by the user after those files were
written. The live firmware is evidence of currently implemented behavior, but
it is not proof that every prototype circuit is suitable for production.

Primary project references:

- [controller/hub HRS](references/hardware_requirements/Hardware_Requirements_Irrigation_Controller_Hub.pdf);
- [current project context](CURRENT_PROJECT_CONTEXT.md);
- [implemented pressure-control node](PRESSURE_CONTROL_NODE.md);
- [commissioned TUF-2000M facts](TUF_2000M_TS2.md);
- [existing pin assignments](PINOUT.md);
- [current RS485 hardware notes](RS485_HARDWARE.md).

Requirement labels used below:

- `REQ` - direct user decision or retained mandatory requirement;
- `PROPOSED` - recommended baseline awaiting owner approval;
- `TBD` - information or validation required before release;
- `OPTION` - supported assembly or product variant;
- `EXCLUDED` - intentionally outside this PCB.

## 2. System-level board families

The irrigation system shall use two production PCB families:

1. the low-power 1S Li-ion solar soil node described in
   `SOIL_NODE_PCB_TECHNICAL_SPEC_DRAFT.md`;
2. this universal nominal-12 V controller for latching valves, pressure,
   flow, level and external industrial equipment interfaces.

`REQ`: the two boards remain electrically and mechanically separate. The soil
node is not expanded into a valve controller, and this board is not optimized
to replace the soil node.

## 3. Product roles and assembly profiles

One PCB layout may support the following distinct build and firmware profiles.
Unused circuits may be DNP to control cost and standby current.

| Profile | Typical power | Populated functions | Runtime mode |
|---|---|---|---|
| `DUAL_PCV` | 12 V lead-acid plus solar charger | two valve drivers, pressure RS485, TUF flow RS485, LoRa | Class A or Class C |
| `LEVEL_FLOW` | 12 V lead-acid plus solar charger | selected RS485 ports, LoRa, optional digital inputs; H-bridges DNP | normally Class A |
| `MAIN_VALVE` | external isolated 230 VAC to 12 VDC PSU | one or two valve drivers, RS485, LoRa | normally Class C |
| `PUMP_VFD` | external isolated 230 VAC to 12 VDC PSU | isolated RS485 to VFD, safety/digital inputs, LoRa; H-bridges DNP unless separately required | Class C |

`REQ`: valve-control firmware and pump/VFD-control firmware remain separate
targets. A common PCB does not authorize common unsafe command semantics.

The present pressure-control hydraulic valve is adjusted mechanically. The PCB
opens/closes its latching solenoid and measures upstream/downstream pressure; it
does not electronically regulate pressure in a closed loop. A future modulating
actuator would be a different validated product profile.

`EXCLUDED`: mains voltage shall not enter this PCB. A rectifier alone does not
convert 230 VAC to 12 VDC. Mains-powered installations require an external,
certified, fused and galvanically isolated SELV 230 VAC to 12 VDC power supply.

## 4. Baseline functional requirements

The maximum-feature build shall provide:

- one ESP32-S3 MCU module;
- one existing SX1262-based DX-LR30 LoRa module and one 50 ohm SMA RF path;
- two independent latching-valve H-bridge channels;
- one selected-at-a-time Modbus RTU master UART routed to up to four protected
  RS485 field branches;
- per-branch switchable field-device power, configured at assembly as 5 V or
  switched battery voltage;
- one controlled auxiliary 5 V output;
- one controlled auxiliary switched-battery output;
- one local 3.3 V I2C expansion connector;
- switched input-voltage measurement;
- protected digital inputs for contacts, float switches, limit switches or
  equipment status;
- an external 3.3 V USB-to-TTL UART0 programming/debug interface with BOOT,
  RESET, DTR and RTS support;
- watchdog, brownout-safe defaults, input protection and diagnostic test points.

## 5. Top-level architecture

```text
12 V lead-acid battery OR external isolated 12 VDC PSU
  -> input fuse / reverse-polarity protection / surge clamp
  -> input voltage monitor and staged undervoltage policy
  -> protected system input bus
       |-> always-on 3.3 V buck -> ESP32-S3 + low-power control logic
       |-> LoRa on 3.3 V, software shutdown between Class A cycles
       |-> valve 1 high-side gate -> H-bridge 1 -> latching valve 1
       |-> valve 2 high-side gate -> H-bridge 2 -> latching valve 2
       |-> protected VBAT_SW auxiliary output
       |-> enable-controlled 5 V buck -> 5V_SW auxiliary output
       `-> selected RS485 branch power

ESP32-S3
  |-> SX1262 / DX-LR30 over SPI
  |-> valve 1 and valve 2 controls, current sense and fault inputs
  |-> one Modbus UART -> branch selector -> one active RS485 branch
  |-> I2C -> expansion connector and low-speed GPIO expander
  |-> switched battery divider -> ADC
  `-> UART0 service header
```

## 6. Input power and lead-acid battery policy

### 6.1 Input range

`REQ`: the board accepts a nominal 12 V DC input from either:

- a 12 V lead-acid battery used with a suitable external solar charge
  controller; or
- an external regulated and isolated 12 VDC power supply.

The HRS working range of 10.5 V to 14.8 V remains a provisional design input.
The final absolute operating and survival ranges are `TBD` from the selected
battery, charger, PSU and field surge environment.

### 6.2 Charging versus load protection

The requested approximately 11.5 V cutoff is an over-discharge/load-protection
function, not overcharge protection.

`REQ`: charging, float voltage, solar-panel tracking and battery-temperature
compensation are owned by an external lead-acid solar charge controller with
mandatory MPPT. PWM-only/fixed-voltage substitutes are not accepted. This
PCB does not claim to be a lead-acid charger.

`REQ`: the PCB protects its own input and loads with:

- replaceable or resettable input overcurrent protection sized from measured
  valve inrush and all connected loads;
- reverse-polarity protection;
- automotive/industrial transient suppression sized from the cable and surge
  environment;
- overvoltage supervision or a defined safe input ceiling;
- hardware undervoltage supervision with hysteresis;
- firmware-visible input voltage and power-fault status.

### 6.3 Staged low-voltage behavior

An immediate whole-board cutoff at 11.5 V is not the baseline because it may
leave a valve open and make the node unable to report its state.

`PROPOSED`: use a staged policy:

1. `LOW_BATTERY_WARN` - report the condition and reduce nonessential activity;
2. `ACTUATION_INHIBIT` - inhibit new OPEN operations while reserving energy for
   one validated CLOSE pulse and a final status uplink;
3. `HARD_UVLO` - hardware disconnect or force all high-power domains off at a
   lower threshold to prevent damaging battery discharge;
4. reconnect only after adequate hysteresis and a stable recovery delay.

The user's 11.5 V proposal is the initial candidate for the actuation-inhibit
level. Exact warn, inhibit, hard-cutoff and reconnect voltages are `TBD` after
measurement under the selected battery, cable and valve pulse load. Thresholds
must account for temporary battery sag during actuation.

`PROPOSED`: provide two resistor/configuration profiles:

- `BATTERY` - staged higher thresholds suitable for lead-acid protection;
- `EXTERNAL_PSU` - lower or altered UVLO so a valid 12 V power supply does not
  unnecessarily block operation.

`PROPOSED`: a low-current 65 V window supervisor such as TPS37-Q1 may supervise
the input and gate enable logic. It does not carry valve current; a correctly
rated high-side MOSFET/eFuse/load-disconnect stage is still required. Final
parts, thresholds, FET SOA and thermal performance are schematic-stage `TBD`.

Official candidate reference:

- [TI TPS37-Q1](https://www.ti.com/product/TPS37-Q1)

## 7. Power rails and controlled outputs

### 7.1 Always-on 3.3 V system rail

`PROPOSED`: use TPS62933 configured for 3.3 V as the primary buck converter.
It supports a wide input range, MCU/radio peak current and low quiescent current.
The exact compensation, inductor, capacitors, thermal area and EMI filtering
shall follow the selected IC datasheet and measured peak load.

- [TI TPS62933](https://www.ti.com/product/TPS62933)

The MCU module, power supervisor and only control logic needed to wake the board
reside on this rail. All optional external loads shall be switchable.

### 7.2 Switched battery output

The output called "12 V" shall be marked `VBAT_SW` on the schematic and PCB.
When powered from a lead-acid battery it is not regulated 12 V and may span the
accepted input range.

`REQ`: `VBAT_SW` shall include:

- default-OFF hardware behavior during MCU reset and boot;
- high-side switching;
- reverse-current blocking where required;
- per-output current limiting or a replaceable/resettable fuse;
- connector-side TVS/ESD protection;
- voltage/current rating for the specified external load;
- fault feedback to the MCU where practical.

The commissioned TUF-2000M accepts approximately 8 V to 36 V and about 50 mA,
so `VBAT_SW` is suitable in principle. Startup time and effects of repeated
power cycling remain validation items.

If a future peripheral truly requires regulated 12.0 V, it requires an
optional buck-boost rail and is not the same as `VBAT_SW`.

### 7.3 Switched 5 V output

`PROPOSED`: use a separate enable-controlled buck stage, provisionally another
TPS62933 configured for 5 V, followed by protected distribution. Its enable
shall have a hardware pull-down so the output remains off through reset.

`TBD`: continuous current, startup surge, output capacitance, cable length and
required hold-up. These determine whether the 5 V converter and output switch
can be combined or require separate protection.

### 7.4 Output safety rule

5 V and `VBAT_SW` selection for a field connector shall be an assembly option,
keyed connector variant or fixed harness choice. Software shall not be able to
apply battery voltage to a connector intended for a 5 V sensor.

## 8. MCU and service interface

### 8.1 MCU choice

`PROPOSED`: use `ESP32-S3-WROOM-1-N8`, or an exact industrial-temperature
no-PSRAM equivalent confirmed at BOM release.

Reasons:

- enough GPIO for two independent valve channels, LoRa, RS485, service UART,
  I2C and diagnostics;
- no external PSRAM is required by the current firmware roles;
- 8 MB flash provides margin for LoRaWAN state, logging and OTA partitions;
- avoiding octal-PSRAM variants preserves GPIO35 through GPIO37 and avoids
  unnecessary standby load;
- official module deep-sleep figures are compatible with a low-power Class A
  build, while Class C power is dominated by the active MCU and receiver.

Strapping GPIO0, GPIO3, GPIO45 and GPIO46 shall not be assigned to field
outputs or peripherals whose state could affect boot. GPIO0 is used only as
the BOOT strap/service input. GPIO3, GPIO45 and GPIO46 remain reserved.

Official references:

- [ESP32-S3-WROOM-1/WROOM-1U datasheet](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
- [ESP32-S3 hardware design guidelines](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)
- [ESP32-S3 UART boot selection](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html)

### 8.2 External USB-to-TTL programming

`REQ`: the deployed PCB does not need an onboard USB-to-UART IC. Provide a
keyed service connector for an external 3.3 V logic-level adapter:

| J_PROG pin | Board signal | Adapter connection |
|---:|---|---|
| 1 | GND | GND |
| 2 | 3V3_REF | High-impedance voltage-reference input only; otherwise leave open |
| 3 | UART0_TX / GPIO43 | adapter RX |
| 4 | UART0_RX / GPIO44 | adapter TX |
| 5 | DTR | adapter DTR, through onboard auto-reset circuit |
| 6 | RTS | adapter RTS, through onboard auto-reset circuit |

This proposed six-pin order is shared with the soil board; it is not a
universal USB-to-TTL cable standard. EN/RESET and GPIO0/BOOT have separate
buttons/test pads. Never connect adapter 3.3 V or 5 V power outputs to
3V3_REF. Power the board from its own DC source; use 3.3 V logic only.

Add the standard Espressif two-transistor DTR/RTS auto-reset circuit. DTR and
RTS are modem-control outputs from the USB-to-UART adapter. The upload tool
toggles them to pulse EN and hold GPIO0 low during reset, entering the ROM
bootloader automatically. Manual BOOT and RESET buttons remain available when
the adapter lacks those signals. The transistor logic prevents the common
DTR=RTS state from holding the MCU permanently in reset.

This same connector supports the serial monitor after upload.

## 9. LoRa radio

`REQ`: use the existing SX1262-based DX-LR30 module family and LoRaWAN, not raw
LoRa. Exact module suffix, regional frequency plan, antenna and legal output
power are `TBD` at release.

The PCB shall include:

- dedicated SPI and radio control GPIOs;
- hardware-defined safe reset and RF-switch states;
- bulk and high-frequency decoupling at the module;
- an RF keepout that follows the module and SMA connector recommendations;
- a controlled 50 ohm single-ended RF trace from the module RF pin to SMA;
- ESD protection selected for low RF capacitance;
- provision to measure active and sleep current.

"Controlled 50 ohm" means the PCB stackup, trace width, copper thickness,
reference plane and spacing are chosen together so the RF interconnect has
approximately 50 ohm characteristic impedance. It is not a 50 ohm resistor in
series with the antenna.

## 10. Dual latching-valve subsystem

### 10.1 Channel independence

`REQ`: provide two electrically independent valve channels. Each channel has:

- a dedicated H-bridge;
- dedicated high-side bridge-rail enable;
- two logic controls for pulse polarity/enable;
- hardware sleep/default-off control;
- fault feedback;
- current-sense feedback;
- local bulk capacitance and transient suppression;
- a keyed two-wire valve connector.

`PROPOSED`: use one DRV8874 per valve rather than L298N-class bipolar bridges.
DRV8874 supports 4.5 V to 37 V, current regulation, IPROPI current sensing,
fault reporting and low-current sleep. Its advertised 6 A value is a peak
rating, not permission to ignore package temperature, FET losses, wiring and
the valve pulse energy.

- [TI DRV8874](https://www.ti.com/product/DRV8874)

### 10.2 Required actuation sequence

Each operation shall follow the already proven safe sequence:

```text
set bridge inputs to neutral
  -> enable the selected valve power rail
  -> wait for rail stabilization
  -> command one validated polarity for a bounded pulse
  -> return bridge to neutral
  -> wait for current decay
  -> put the driver to sleep
  -> disable the selected valve power rail
  -> verify and report current/fault result
```

The existing firmware implements this sequence for one valve. It does not yet
implement two valve objects/channels.

`TBD` for each actual valve model:

- OPEN polarity;
- CLOSE polarity;
- pulse duration;
- expected pulse-current envelope;
- current limit;
- cable voltage drop;
- bulk capacitance;
- minimum battery voltage for a reliable close;
- allowed repetition rate and thermal recovery.

`PROPOSED`: forbid simultaneous bridge pulses in the baseline firmware. Queue
operations and actuate one valve at a time to reduce battery sag, fuse size and
thermal stress. Whether simultaneous operation is ever allowed remains `TBD`.

### 10.3 Reset and fault safety

All H-bridge inputs, nSLEEP inputs and rail-enable controls shall have hardware
pull states that guarantee neutral/default-OFF behavior while the ESP32 is
unpowered, reset, booting or being programmed. Firmware initialization alone
is insufficient.

Overcurrent, open-load/low-current and driver fault results shall be visible in
telemetry. Lack of confirmed mechanical position feedback shall be reported as
"commanded state", not asserted as verified physical state.

## 11. RS485 and Modbus architecture

### 11.1 Retained commissioned behavior

The live TUF-2000M facts shall be preserved:

- Modbus RTU address 1;
- 9600 baud, 8N1;
- hardware-confirmed `LOW_WORD_FIRST` REAL4 layout;
- on-demand polling of flow rate, velocity and read-only accumulated totals;
- no invented Modbus total-reset write and no M37 factory-erase operation.

Pressure sensors are currently I2C prototypes. Their future RS485 production
model, address, supply voltage, current, register map, scaling and startup time
remain `TBD` and shall not be described as already implemented.

### 11.2 Four-branch topology

The HRS concept of four isolated branches tied directly to one UART as an
"active star" is not accepted without additional selection logic. Multiple
push-pull receiver outputs cannot be tied together safely, and an RS485 star is
not automatically a transparent hub.

`PROPOSED`: use one Modbus-master UART and a break-before-make branch selector:

```text
ESP32 UART
  -> branch selection / receive mux
      |-> protected RS485 port 1
      |-> protected RS485 port 2
      |-> protected RS485 port 3
      `-> protected RS485 port 4
```

Only one branch transceiver and its field power may be enabled at a time. This
is suitable because the controller polls Modbus slave devices sequentially.

The reference `DUAL_PCV` harness assignment is:

| Port | Default role | Power option |
|---:|---|---|
| RS485-1 | upstream pressure sensor | fixed by sensor BOM: 5 V or `VBAT_SW` |
| RS485-2 | downstream pressure sensor | fixed by sensor BOM: 5 V or `VBAT_SW` |
| RS485-3 | TUF-2000M flow meter | `VBAT_SW` |
| RS485-4 | spare level/flow/service instrument | fixed assembly option |

Port numbers are connector roles, not assumed Modbus addresses. If several
devices later share one multidrop cable, they still require unique addresses
and a linear terminated bus rather than a passive star.

RS485 sensor polling is not "always listening" in the normal controller role.
LoRaWAN Class C is the interface that remains continuously receptive to network
commands. If a future product must itself act as an always-listening Modbus
slave, it needs a dedicated always-on RS485 service-port design or an explicit
assembly variant.

### 11.3 Direction control options

`REQ`: retain automatic RS485 direction switching in the production baseline,
matching the commissioned prototype behavior and the user's earlier decision.
No ESP32 DE/RE pin is allocated in the preliminary pin map.

The preferred implementation and one contingency are:

1. `PROPOSED-A`, baseline: isolated automatic-direction transceiver such as MAX22025F /
   MAX22026F plus a separately sized isolated cable-side supply;
2. `PROPOSED-B`, contingency only: ISOW1412 with integrated isolated power and
   explicit DE/RE firmware control, requiring owner approval to change the
   automatic-direction requirement.

The current prototype's 74HC14 automatic-direction circuit is valid evidence
that automatic direction can work at 9600 baud. It is not by itself a complete
isolated, surge-protected multiport production solution.

Official candidates:

- [ADI MAX22025F/MAX22026F](https://www.analog.com/en/products/max22026f.html)
- [TI ISOW1412](https://www.ti.com/product/ISOW1412)

`TBD`: qualify the automatic-direction option at all required baud rates and
confirm isolation voltage, EMC, isolated-power efficiency, available package,
cost and standby current. If it fails validation, do not silently switch to
manual DE/RE; return the tradeoff for approval.

### 11.4 Isolation boundary

The port must be classified explicitly as one of the following:

- `FULL_ISOLATION`: isolated logic plus isolated field-device power/return, or
  externally powered equipment with no common DC return to the PCB;
- `SIGNAL_ISOLATION_ONLY`: isolated data path but common 5 V/VBAT return used to
  power the field device.

Supplying a sensor from the board's common battery and common ground defeats a
claim of full galvanic port isolation. The PCB and BOM must not use ambiguous
"isolated RS485" language without declaring the power-return boundary.

For a fully isolated, board-powered TUF-2000M branch, the isolated DC/DC shall
be sized above its approximate 0.6 W steady load with startup and temperature
margin. A nominal 1 W converter may be marginal; the exact rail should be
selected after measurement, with 2 W as a reasonable evaluation class rather
than a released requirement.

### 11.5 Port details

Each RS485 connector shall provide:

- keyed `PWR`, `RETURN`, `A`, `B`, and shield/drain positions;
- connector-side surge/ESD protection;
- optional/DNP 120 ohm termination;
- optional/DNP bias components;
- clearly documented cable shield and chassis/earth strategy;
- per-port power enable and fault/current-limiting protection;
- test points on both logic and cable sides where isolation is populated.

Termination shall be populated only where the board is physically at a bus
end. Stub length and cable topology remain installation constraints.

## 12. I2C expansion

`REQ`: provide one local expansion connector with:

- switched/current-limited 3.3 V;
- GND;
- SDA;
- SCL.

Include ESD protection and DNP pull-up footprints. The pull-up value and bus
speed are `TBD` from connected devices and total capacitance.

This I2C port is for short, enclosure-local wiring. It is not an outdoor field
bus and shall not be routed over long unshielded cables.

## 13. Additional reusable I/O

`PROPOSED`: provide at least four protected digital input channels for:

- dry contacts;
- float/level switches;
- valve limit switches or position contacts;
- VFD RUN/FAULT feedback;
- emergency-stop or permissive-loop status;
- slow equipment-state contacts.

The selected expander inputs are not guaranteed lossless pulse counters.
Meter pulse counting or asynchronous deep-sleep wake requires a separately
reviewed counter/direct-RTC-GPIO option; see Section 14.3.

Final input voltage class is `TBD`. For maximum reuse, use an assembly option
for isolated 12/24 V industrial inputs rather than connecting field contacts
directly to ESP32 GPIOs.

`PROPOSED`: reserve one isolated dry-contact or open-drain control-output option
for external equipment permissive/stop interfacing. Direct motor or mains
switching is `EXCLUDED` from this PCB.

For a pump/VFD build, hardwired safety interlocks and the VFD's own safe-stop
chain take precedence over LoRa commands. Loss of MCU, radio or network must
resolve to a defined safe state.

## 14. ESP32-S3 pinout and electrical constraints

Pin-map revision: `S3-P2`, reviewed 2026-09-04. Exact baseline module:
`ESP32-S3-WROOM-1-N8`, 8 MB Quad SPI flash, NO PSRAM.

This supersedes the Draft 0.1 GPIO allocation. GPIO39-42 are now reserved for
pad JTAG rather than valve control. Both bridge pulse enables and both power
gates remain direct MCU outputs. Slow permissions/faults use one proposed
`TCA6424A` 24-bit I2C expander, fully allocated in Section 14.3.

A GPIO number is NOT a physical module pad number. Pads below are WROOM-1
pads, not bare-chip pins or DevKit header positions. SPI and UART1 are routed
explicitly through the GPIO matrix, regardless of peripheral default pins.

Types: A = analog input; I = input; O = output; OD = open-drain; - = reserved.
L/H are required circuit levels, not guaranteed bare-chip reset levels.
PU/PD are external pulls; listed resistor values are starting points.

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

### 14.1 Module power and pins not used for field functions

- Pad 2: 3.3 V; pad 3: EN/CHIP_PU with reset supervision and RC.
- Pads 1, 40 and exposed pad 41: GND.
- GPIO22-34 are not available as module-header GPIOs; this range includes
  nonexistent chip numbers and internal flash/unexposed signals. Do not route
  them as expansion pins.
- GPIO35-37 are usable in this exact N8 no-PSRAM build. Substituting an
  octal-PSRAM module such as N8R8 invalidates the map.
- GPIO47/48 are 3.3 V in the selected N8 module; they are 1.8 V on the R16V
  variant. No R16V or other module substitution is allowed without review.
- GPIO0/3/45/46 are straps: BOOT=GPIO0, GPIO3 JTAG-source strap, GPIO45 flash
  supply selection and GPIO46 boot/ROM control. Keep GPIO0/3 HIGH and
  GPIO45/46 LOW with the reviewed pull network; do not place field loads here.
  The meaning of straps depends on eFuses; read and record the actual factory
  configuration. Do not program eFuses as a pinout shortcut.
- GPIO39-42 remain DNP/debug-only JTAG pads, not valve or auxiliary outputs.
- GPIO19/20 remain USB/debug-only. USB startup pulses make them unsuitable
  for unguarded power enables in this board.
- GPIO15/16 are allocated to LoRa: no external 32.768 kHz crystal on them.
- GPIO43/44 are dedicated UART0; the ROM TX stream must never energize a load.

### 14.2 LoRa cross-reference and direct valve controls

| DX-LR30 signal | Radio module pin from project reference | ESP32-S3 GPIO / module pad |
|---|---:|---|
| RXEN | 6 | 16 / 9 |
| TXEN | 7 | 17 / 10 |
| DIO1 | 13 | 13 / 21 |
| BUSY | 14 | 14 / 22 |
| RESET | 15 | 15 / 8 |
| MISO | 16 | 11 / 19 |
| MOSI | 17 | 10 / 18 |
| SCK | 18 | 9 / 17 |
| NSS | 19 | 12 / 20 |

DX-LR30 VCC is 3.3 V, not GPIO-powered; DIO2/DIO3 stay unconnected unless the
actual purchased module requires otherwise. The radio remains on 3.3 V in the
baseline and uses software shutdown; there is no unallocated radio-power gate.

| Channel | PH | EN / bounded pulse | High-side power request | nSLEEP permission | nFAULT |
|---|---|---|---|---|---|
| Valve 1 | GPIO5 | GPIO6 | GPIO35 | TCA6424A P14, interlocked | TCA6424A P24 |
| Valve 2 | GPIO38 | GPIO47 | GPIO36 | TCA6424A P15, interlocked | TCA6424A P25 |

Use DRV8874 in the explicitly strapped PH/EN control mode. PH chooses polarity;
EN is not permanently HIGH. nSLEEP, EN and rail-enable circuits default LOW.
OPEN/CLOSE polarity, pulse duration and driver current limits are still TBD.

### 14.3 Fully assigned low-speed I/O expander

`PROPOSED`: TCA6424A in RGJ 32-pin package. VCCI pin 31 and VCCP pin 27
connect to always-on 3.3 V; GND pin 25 and the exposed pad connect to GND.
ADDR pin 26 is LOW: 7-bit I2C address `0x22` (reserve it on the expansion bus).
SCL pin 29 goes to GPIO7; SDA pin 30 to GPIO8. INT_N pin 32 goes to GPIO37.

RESET_N pin 28 shall follow the reviewed board reset/watchdog network. It is
not sufficient to leave the expander alive across a CPU fault with old
permissions retained. Its RESET/EN fanout and brownout timing require schematic
validation; an internal MCU software reset does not necessarily pull EN LOW.

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

P00-P17 are outputs only after initialization, with external pull-downs
(start at 100 kOhm). P20-P27 remain protected inputs; weak pull-ups start at
100 kOhm and require noise/leakage review. Clamp/buffer nFAULT paths for
powered-off drivers. Pull-up current from active-low faults/contacts must be
included in the sleep budget.

Important: TCA6424A powers up with all pins configured as INPUTS, but its output
latches default to ones. Write output registers 0x04, 0x05 and 0x06 to 0x00
and verify them BEFORE setting configuration 0x0C=0x00, 0x0D=0x00, 0x0E=0xFF.
Keep OUTPUT_ARM=0 until all direct MCU outputs and permissions are safe.
Otherwise changing pin direction first can momentarily enable every load.
On I2C error, abort the operation and drop the direct bridge gates/enables.

P20-P23 are state/contact inputs, not guaranteed lossless pulse counters.
GPIO37 is not RTC-capable: this INT wiring does not wake the MCU from deep
sleep. Class A uses timer wake; asynchronous contact wake or accurate pulse
counting requires a revised GPIO/counter design. No GPIO status LED or separate
service-wake button is allocated on this board; BOOT and RESET remain available.

### 14.4 Enable routing and hardware interlocks

- P07 enables the 5 V converter; P10 independently switches the auxiliary 5 V
  connector. If a sensor port uses 5 V, enable P07 and its port switch without
  necessarily enabling P10.
- P11 enables the protected VBAT auxiliary connector.
- P12 enables local I2C connector power AND its bus isolation. Keep the
  expander/internal I2C pull-ups on always-on 3.3 V; external unpowered devices
  must not clamp or back-power the internal control bus.
- P13 enables the high-side battery divider; GPIO1 reads it. Unlike the soil
  board, universal-board GPIO1 is an ADC input, not a divider-enable output.
- P00/P01 choose port 1/2/3/4 as 00/01/10/11 (SEL1:SEL0).
  P02=0 disconnects all UART branches. It is NOT DE/RE direction control.
  Hardware-decode P03-P06 requests with the selected port and global permission;
  forbid more than one powered/connected branch. Change selection only with
  BUS_EN=0 and after the previous transaction and field-power shutdown.
- GPIO35/36 are requests to the valve high-side gates, not direct FET-gate
  connections. The driver circuit must also require OUTPUT_ARM, valid power
  and reset/watchdog permission. P14/P15 release nSLEEP only for a powered,
  permitted channel. No I2C command alone may energize a bridge.
- All bridge PH/EN inputs stay neutral while rail and nSLEEP settle. Serialize
  valve pulses; deassert EN, let current decay, assert sleep, then remove rail.
- P17 drives only an isolated low-voltage equipment interface. It is not a
  certified emergency stop; hardwired VFD safety remains independent.

### 14.5 Boot, sleep and failure tests

Espressif specifies startup glitches even on several non-strapping GPIOs.
Pull resistors do not override every actively driven glitch. Keep valve rails
and enables hardware-blocked throughout boot, download and reset; reinitialize
the radio after MCU restart and keep all RS485 branches isolated during boot.

Before Class A sleep: disable bridge EN and PH outputs, assert both nSLEEP
permissions LOW, drop both power gates, all branch/AUX/5 V/I2C enables,
VBAT_DIV_EN and OUTPUT_ARM. Put LoRa into shutdown and hold its control states.
Keep the expander powered with its OFF outputs, disable unused input buffers
and use the supported S3 digital-pad deep-sleep hold where needed. GPIO35-48 are
not RTC GPIOs; do not claim that they can toggle or provide EXT1 wake in sleep.

The Class C receiver and MCU stay awake; only unused field/bridge rails switch
off. Serial and Class C are not subject to a deep-sleep-current claim.

Required tests include power ramp, EN reset, watchdog/software reset, brownout,
UART download, I2C bus stuck LOW, expander-reset latch behavior, disabled-rail
back-power, and wake/hold release. No firmware or manufacturing pinout is
released until these tests and the exact schematic/footprints are approved.

### 14.6 Official pinout sources

- [ESP32-S3-WROOM-1 datasheet v1.8, Sections 3-4](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf):
  module pads, variant restrictions and boot straps.
- [S3 schematic checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html):
  reset, startup glitches, USB and ADC constraints.
- [TI TCA6424A datasheet Rev D](https://www.ti.com/lit/ds/symlink/tca6424a.pdf):
  RGJ pin numbers, address and reset/register behavior.

The allocation and external interlocks are project design proposals, not a
claim of manufacturer certification.

## 15. Battery measurement

Use the ESP32 ADC through a high-impedance divider that is disconnected by a
low-leakage high-side switch except during measurement. Include input filtering,
ADC calibration support and a test point.

Firmware shall report:

- measured input voltage;
- low-battery state;
- actuation-inhibit state;
- hard-UVLO/restart cause when detectable;
- configured input profile (`BATTERY` or `EXTERNAL_PSU`).

Voltage alone shall not be presented as an accurate lead-acid state-of-charge
percentage under load.

## 16. Runtime and LoRaWAN power modes

### 16.1 Serial-only target

The MCU remains awake for commissioning and local diagnostic commands. This is
not a low-power deployed mode.

### 16.2 LoRaWAN Class C target

Class C keeps the MCU, serial subsystem needed by the target and LoRa receiver
continuously active. The configured interval is only a telemetry interval; it
is not sleep time. High-power field rails, H-bridges and unused RS485 branches
shall still remain off between operations.

Class C is the normal candidate for external-PSU main-valve and pump/VFD sites.

### 16.3 LoRaWAN Class A target

Class A performs a bounded wake/read/actuate/transmit/RX1/RX2 cycle and then
enters real timer deep sleep. Valid commands require immediate application
acknowledgement before sleep. The configured interval is actual sleep time.

Class A is the normal candidate for solar battery level/flow stations and may
be used for the pressure/valve node only if command latency through scheduled
uplinks is acceptable.

`REQ`: the board shall not promise "commands at any moment" while operating in
Class A. The next command opportunity occurs only after a wake/uplink and its
RX windows.

## 17. Standby-current target

The old HRS target of at most 50 uA remains an engineering goal for a minimal
Class A assembly, not yet a guaranteed maximum for the fully populated board.

The budget must include, at minimum:

- ESP32-S3 module deep sleep;
- 3.3 V buck quiescent current;
- input supervisor and reverse-protection leakage;
- LoRa shutdown leakage;
- GPIO expander and pull networks;
- disabled 5 V buck and high-side-switch leakage;
- disabled valve drivers and gates;
- disabled RS485 branches and isolated DC/DC leakage;
- battery divider switch leakage;
- LEDs, ESD devices and connector contamination margin.

`REQ`: no indicator LED may be continuously energized in the low-power build.

`TBD`: define separate measured limits for:

- `LEVEL_FLOW_CLASS_A_MINIMAL` assembly;
- `DUAL_PCV_CLASS_A` assembly;
- `CLASS_C` active idle;
- each enabled sensor/RS485 rail;
- each valve pulse.

The board shall include a removable current-measurement link or jumper so sleep
current can be measured without cutting PCB traces.

## 18. PCB, EMC and connector requirements

`PROPOSED`: use a four-layer PCB for the maximum-feature board:

1. components and signals;
2. continuous ground/reference plane with intentional isolation clearances;
3. power distribution and slow signals;
4. components and signals.

Four layers are recommended because this board combines high-current pulsed
H-bridges, multiple switching supplies, isolated field buses and a 50 ohm RF
path. A two-layer build may be evaluated only after routing and EMC review; it
must not break the RF return path or mix valve-current returns with sensitive
ADC/radio grounds.

Layout shall provide:

- short valve-current loops and local bulk capacitance;
- separated switching-node copper from ADC, crystal and RF routing;
- one controlled return strategy between power input, bridge currents and MCU;
- isolation creepage/clearance appropriate to the declared port rating;
- no copper beneath RF keepout areas where prohibited;
- SMA at the board edge with ground stitching;
- TVS devices close to field connectors;
- shield termination that does not inject surge current through logic ground;
- readable connector voltage labels, especially `5V_SW` versus `VBAT_SW`;
- test points for all rails, UART, RS485 A/B, bridge outputs, faults and enables.

## 19. Firmware changes required after hardware approval

The current code is not modified by this document. Later firmware work shall:

- extend one latching-valve driver instance to two independent channels;
- preserve the safe neutral/power/pulse/neutral/off sequence;
- serialize valve pulses unless simultaneous operation is explicitly approved;
- add current-envelope and nFAULT diagnostics;
- add selected-branch RS485 routing and per-port power sequencing;
- retain TUF-2000M address, baud, word order and non-destructive total handling;
- implement the actual selected RS485 pressure-sensor driver only after its
  manual and hardware are available;
- implement staged battery warnings, OPEN inhibit and reserved CLOSE energy;
- maintain separate Serial, Class C and Class A targets;
- add distinct product profiles so a VFD node cannot execute valve-only logic;
- report board revision, population profile and pin-map revision in telemetry.

## 20. Acceptance tests before schematic release

### 20.1 Design review

- exact MCU and DX-LR30 module suffixes are confirmed;
- every ESP32 strapping/default state is documented;
- both H-bridge channels are safe during power-up, reset and programming;
- input profile and UVLO resistor options are reviewed;
- RS485 isolation boundary is explicit for every assembly profile;
- no two RS485 receiver outputs can electrically contend;
- 5 V and VBAT sensor connectors cannot be confused;
- all DNP/assembly options have unambiguous BOM codes.

### 20.2 Bench validation

- cold crank/battery sag and slow-ramp input tests;
- reverse input and overvoltage/surge tests at defined levels;
- reliable open and close at minimum allowed battery voltage and longest cable;
- stalled/shorted/open valve fault tests on both channels;
- repeated pulse thermal test;
- proof that simultaneous pulses are blocked;
- four-port RS485 selection, termination and power-cycle tests;
- TUF-2000M reads with hardware-confirmed word order;
- selected pressure/level sensor startup and Modbus tests;
- UART0 manual and DTR/RTS automatic programming tests;
- Class A sleep-current measurement by assembly profile;
- Class C active-idle and radio-current measurement;
- conducted/radiated EMC pre-scan with valve pulses and DC/DC converters active;
- LoRa output and receive test through the final SMA path.

### 20.3 Field-safety validation

- loss of radio/network during OPEN/CLOSE operation;
- brownout during a valve pulse;
- MCU watchdog reset while a bridge is enabled;
- VFD communication loss and defined fail-safe response;
- recovery after battery recharge or external PSU restoration;
- connector miswiring cases included in the product installation instructions.

## 21. Release blockers and owner decisions

The following items remain `TBD` before schematic/BOM release:

1. exact two-wire latching valve model, pulse current, duration and polarity;
2. whether simultaneous valve pulses are permanently prohibited;
3. exact RS485 pressure-sensor model, supply, current and Modbus map;
4. number and power requirements of level and other field sensors;
5. required auxiliary 5 V and VBAT output currents and cable lengths;
6. full isolation versus signal-only isolation per RS485 port/profile;
7. exact qualified automatic-direction transceiver and isolated-power design;
8. required RS485 baud rates beyond the commissioned 9600 baud TUF meter;
9. battery model/capacity, solar charger/controller and operating temperature;
10. warn, OPEN-inhibit, hard-UVLO and reconnect thresholds;
11. required 12/24 V digital-input count and type;
12. VFD brand/model, Modbus map and hardwired permissive/safe-stop requirements;
13. LoRa regional plan, antenna, enclosure and cable arrangement;
14. connector family, IP rating, grounding and surge environment;
15. sleep-current target for each populated Class A build rather than one number
    for mutually different assemblies.

## 22. Recommended approval baseline

For owner review, the recommended baseline is:

- one universal ESP32-S3-WROOM-1-N8 PCB as the second board family;
- four assembly/firmware profiles, not one combined runtime;
- external certified 230 VAC to 12 VDC PSU for mains installations;
- external lead-acid solar charger, with this PCB responsible for staged load
  undervoltage protection;
- two independent DRV8874 valve channels with high-side rail gating, current
  sense, nFAULT and serialized pulses;
- four protected RS485 connectors, one branch active at a time;
- explicit isolation classification and fixed per-port 5 V or VBAT assembly;
- controlled `5V_SW` and protected `VBAT_SW` auxiliary outputs;
- local switched 3.3 V I2C and protected general-purpose field inputs;
- existing DX-LR30 LoRa module with a controlled 50 ohm SMA path;
- external UART0 USB-to-TTL programming with DTR/RTS auto-reset plus manual
  BOOT/RESET;
- separate Serial-only, LoRaWAN Class C and LoRaWAN Class A firmware targets.

Approval of this baseline authorizes detailed schematic capture. It does not
close the release blockers in Section 21.
