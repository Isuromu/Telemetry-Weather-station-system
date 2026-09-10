# Current Project Context

This file records direct user clarifications made after the preserved Codex
handoff was created. It is the current source of truth when it differs from the
archived handoff package.

## System scope

The irrigation automation project is a complex system with multiple distinct
end nodes. The pressure-control/valve node is one end-node type within that
larger system; the repository must not be modeled as though the entire project
were only one valve controller.

Node-specific hardware and behavior should remain separated from reusable
drivers, shared protocols, and common telemetry structures. Avoid ambiguous
generic names that mix different node or actuator types.

## Valve-control and monitoring end node

The current valve end node is expected to handle:

- control of the latching `PressureControlValve` solenoid;
- upstream pressure measurement;
- downstream pressure measurement;
- battery voltage/level monitoring;
- a TUF-2000M ultrasonic flow meter with TS-2 clamp-on transducers over RS485.

The two pressure sensors are currently connected through I2C. This is the
present prototype arrangement and must continue to work until the hardware is
actually migrated.

The end node does not currently regulate or set hydraulic pressure. The
latching solenoid only opens or closes the valve. Pressure is adjusted manually
on the hydraulic valve, while the firmware measures upstream/downstream
pressure for monitoring and diagnostics. Names and documentation must not imply
closed-loop electronic pressure regulation.

## Planned sensor transport

The long-term plan is for the complete project to read all sensors through
RS485. The migration must be treated as a planned architecture, not as a claim
that the current I2C pressure sensors have already been replaced.

The TUF-2000M is needed only for a periodic/on-demand water flow measurement;
continuous reading is not required. GPIO16/GPIO17 are now assigned to UART2
for its auto-direction RS485 converter. The installed sensor pair is TS-2
(small), documented for DN25-100 and -30 to 90 degrees C. The installed pipe
is marked PVC-U W/P, 63 x 3 mm, PN10: 63 mm outside diameter, 3 mm wall and
57 mm calculated inside diameter. The meter is configured for water, no
liner, V-method, and reports an M25 inner transducer spacing of 39.655 mm.

The supplied manual confirms an 8-36 VDC supply at approximately 50 mA, so the
installed 12 V battery is within range and a 24 V boost converter is not
required by this manual. It also confirms RS485 485+/485- terminals, serial
setup in M62, and protocol selection in M63. M63 must be changed from its
factory-default MODBUS ASCII mode to MODBUS_RTU. The technical manual confirms
function 03, zero-based Modbus addressing, flow rate at REG0001-0002 in m3/h,
velocity at REG0005-0006 in m/s, error bits at REG0072, and factory serial
framing 9600 8N1. The firmware now implements these on-demand reads. See
`TUF_2000M_TS2.md`.

The user confirmed the commissioned communication settings as M46 address 1,
M62 9600 none 8 1, and M63 MODBUS_RTU. A direct hardware read confirmed
`LOW_WORD_FIRST`: raw REG0221-REG0222 `00 00 42 64` becomes IEEE-754
`42 64 00 00` = the configured 57.0 mm inner diameter. Normal flow telemetry
therefore uses `LOW_WORD_FIRST`; Serial `flow probe` remains a raw diagnostic.

An earlier installation check reported M08 value 4 (`poor received signal`),
Q = 2, and M91 about 77.56 percent. The user has since confirmed that the
TUF-2000M itself shows flow/velocity and the ESP32 successfully reads both.
The complete ESP32-to-RS485-to-meter path is therefore commissioned; future
installation changes should still recheck M08, M90, and M91.

The project now reads accumulated volume from REG0113-REG0118: net, positive,
and negative totals are direct `REAL4` cubic-metre values. `flow total reset`
does not erase the meter. It stores the current positive total as an ESP32 NVS
baseline and reports delivered water since that baseline. This survives reset
and Class A deep sleep and avoids undocumented/destructive Modbus writes.

## Battery and energy requirement

This end node uses a large battery, and its voltage/level must be monitored.
The presence of a large battery does not relax the power requirement: the
complete design should be as energy-efficient as practical.

Energy use must therefore be considered across the whole operating cycle:

- keep the latching solenoid and L298N unpowered except during required pulses;
- avoid unnecessary continuous polling, logging, or radio activity;
- use configurable sampling and telemetry intervals;
- place the ESP32 and peripherals into appropriate low-power states when the
  node's control and communications duties permit it;
- account for idle consumption of voltage dividers, RS485 hardware, radio,
  sensors, regulators, and power indicators;
- power-gate peripherals where safe and supported by the actual hardware;
- batch sensor acquisition and radio transmissions where this does not harm
  control response or safety;
- measure real sleep, idle, sensing, radio, RS485, and valve-pulse currents on
  the assembled hardware rather than relying only on nominal datasheet values.

### Measured battery-divider reference

The user measured 12.43-12.44 V at the battery and 2.097 V directly at ESP32
GPIO35. The upper divider resistor is remembered as 100 kOhm; the measured
ratio of 5.9299 is consistent with a nominal 20 kOhm lower resistor, not the
previously assumed 22 kOhm. A 100 nF capacitor is fitted from the ADC input to
ground.

The active configuration therefore uses 100 kOhm / 20 kOhm and a calibration
factor of 0.9883, which maps 2.097 V to 12.435 V. The Serial `battery` command
prints both the ADC-pin voltage and reconstructed battery voltage so this can
be checked against the same multimeter. Confirm the lower-resistor marking when
the hardware is accessible; the calibration remains a single measured-point
calibration until additional voltages are checked.

The node now has three explicit power/communication policies. `pcv_serial_only`
keeps local Serial control and never initializes LoRaWAN.
`pcv_hybrid_class_c` keeps ESP32, Serial, and the SX1262 Class C receiver active
so Gateway commands can arrive without waiting for an uplink; its interval is
only the periodic telemetry interval. `pcv_low_power_class_a` follows the soil
node pattern: wake, measure, uplink, receive a queued RX1/RX2 command, execute,
send an immediate result uplink, and enter timer deep sleep; its interval is
real sleep time. The valve_1 commissioning build temporarily defaults to 10
seconds and accepts 10-86400 seconds. This is a bench-test exception; restore
the default and minimum to at least 60 seconds before field deployment. Class C
deliberately trades energy for command latency and retains its 60-second
minimum.

The user confirmed that the working `klapan.zip` project is the separate
valve_2 device without a flow meter. It uses LoRaWAN Class C and its own older
FPort-10 codec. valve_1 is a distinct Class A device with the commissioned
TUF-2000M flow meter and the repository's FPort-30/FPort-31 protocol. Do not
reuse valve_2 credentials, payloads, codec, or continuous-receive behavior for
valve_1.

valve_1 is registered under ChirpStack 4.18.0 with Mosquitto 2.0.21. Its
application ID, DevEUI, JoinEUI and unique AppKey are stored only in the
Git-ignored local secrets header and ChirpStack configuration; do not copy
them into tracked documentation.

## Current valve command and feedback behavior

GPIO16/GPIO17 have been released for UART2 by moving the H-bridge inputs to
GPIO2/GPIO15. These are strapping pins and use the installed 20 kOhm pull-downs; GPIO27
also requires a hardware pull-down that guarantees bridge power OFF during
reset. Boot initializes the H-bridge inputs and switched power to their safe inactive
state and sends no OPEN or CLOSE pulse. Every explicit `pcv open` or `pcv close`
command may send a latching pulse, including a repeated command for the same
last-commanded state. Firmware records and persists only the command it successfully
generated; without position feedback the physical valve remains `UNKNOWN` and
verification remains `NOT_AVAILABLE`.

A future ChirpStack-connected gateway may infer valve behavior from pressure
only after both sensors have validated scales and the hydraulic conditions make
the inference meaningful. Current test logs show the upstream channel pinned at
-10 bar and about -10 degrees C while downstream is near 0.02 bar and 29 degrees
C. The upstream channel is therefore not usable for gateway inference yet; its
sensor documentation, raw values, wiring, and range must be checked first.

## Still unresolved

Do not assume or invent:

- the final set and roles of every end-node type;
- the final RS485 topology and addressing plan;
- the TUF-2000M power-up timing;
- how battery percentage/state-of-charge will be derived from voltage;
- the final deployed reporting interval (10 seconds is commissioning-only and
  the deployment value remains undecided);
- credentials for end nodes other than the provisioned valve_1 device;
- final energy budget and acceptable command latency.

These values require the actual hardware, manuals, system behavior, or a direct
user decision.

## Current repository implementation

The live repository now keeps the existing pump/VFD firmware and the
pressure-control firmware as separate PlatformIO targets. The three valve-node
targets are `pcv_serial_only`, `pcv_hybrid_class_c`, and
`pcv_low_power_class_a`; they are documented in `PRESSURE_CONTROL_NODE.md`.

The TUF-2000M Modbus RTU driver is implemented, including protocol tests based
on the manual's example frame. The Prototype Rev A application uses UART2
GPIO16/GPIO17, commissioned unit 1, and the hardware-confirmed
`LOW_WORD_FIRST` decoder. Normal decoded readings and `flow probe` are enabled.
Accumulated net/positive/negative cubic-metre totals and the persistent local
baseline reset are implemented for Serial and both LoRaWAN modes.
LoRaWAN Class C continuous receive, low-power Class A with
command acknowledgement, RTC session retention, NVS command deduplication, and
separate variant-named ChirpStack codecs are implemented for this node.

The project-accessible `tools/USB_RS485_Sensor_Tool_v0_9_2/` utility contains a
standalone Windows build with a TUF-2000M + TS-2 profile. One read operation
captures and parses flow/velocity, REG0113-0118 volume accumulators,
REG0072/M08, REG0092-0094/M90,
REG0097-0098/M91, REG0221-0222 inner diameter, and REG1442/M46 while retaining
every complete TX/RX frame. Its TUF profile now uses the hardware-confirmed
`LOW_WORD_FIRST` decoder.

## Preliminary production PCB planning

Two hardware-requirements PDFs supplied on 2026-08-29 describe the intended
production board families: a small solar soil-monitoring node and a larger
dual-latching-valve/multi-channel controller. Project-local preserved copies
are under `docs/references/hardware_requirements/`.

The user currently prefers a simple first soil-node PCB with one explicitly
selected 1S 3.7 V Li-ion chemistry, ESP32-C6, the supplied SX1262/DX-LR30
module, one switched 5 V RS485 soil-probe rail, a mechanical system disconnect,
temperature-qualified solar charging, and a switched ESP32 ADC divider. The
cell must be removable: the PCBA contains a mechanically retained 18650 holder,
not a welded battery pack. The latest user correction makes GPS/GNSS and MPPT
mandatory; the earlier GPS exclusion and LTC4079 choice are superseded.
An external RTC, ADS1115, and a fuel-gauge IC remain excluded.
The charger remains connected to the panel and battery
while the system switch is OFF.

RS485 direction for this soil node must be automatic, matching the user's
existing converter concept. No MCU `DE`/`RE` GPIO is allocated. One
`SENSOR_DOMAIN_EN` signal switches both the 5 V boost and the RS485 transceiver
domain; `VBAT_DIV_EN` separately switches the battery divider.

The superseded C6-P1 assignment used GPIO0/1 for battery ADC and
divider enable, GPIO2 for the sensor domain, GPIO3/7 for LoRa RXEN/TXEN,
GPIO6 for service wake, GPIO12/13 for RS485 TX/RX, GPIO14 for LoRa reset,
GPIO16/17 as dedicated UART0 programming/Serial pins, and GPIO18-23 for LoRa
SPI/NSS/DIO1/BUSY. GPIO4/5/8/15 are reserved strapping/JTAG pins; GPIO9 remains
BOOT. Native USB and an on-board USB-to-UART bridge are not fitted. A service
header exposes 3.3 V UART0, GND, 3V3 reference, DTR and RTS for an external
USB-to-TTL adapter. DTR/RTS use the standard two-transistor automatic
boot/reset circuit; independent BOOT/RESET buttons or pads remain available
for simple adapters without modem-control outputs.

The earlier 5 uA requirement was a user correction/error and is withdrawn. The
current requirement is <=25 uA for the complete assembled soil node in normal
ESP32-C6 timer deep sleep. Every inactive or failure-backoff state must remain
below 1 mA with sensor, RS485 and the battery divider hardware-OFF. Intentional
boot, soil measurement, LoRa TX/RX, UART programming and battery charging are
excluded from the 1 mA idle limit because their required currents are
necessarily higher.

The shorter decision record is in
`SOIL_NODE_PCB_PRELIMINARY_SPEC.md`. The detailed architecture-review basis for
the contractor HRS, including traceability, proposed components, provisional
GPIO allocation, acceptance tests and approval gates, is in
`SOIL_NODE_PCB_TECHNICAL_SPEC_DRAFT.md`. Recommendations in that review draft
remain proposals until the user approves them.

## Universal 12 V production controller planning

The user confirmed that the second production PCB family shall be one
universal nominal-12 V controller with assembly and firmware variants for:

- a two-latching-valve pressure/flow node;
- a battery/solar RS485 level or standalone-flow node;
- a mains-powered main-valve node;
- a mains-powered pump/VFD interface node.

The same bare PCB may serve these roles, but valve and pump/VFD command logic
must remain separate firmware targets. Unused H-bridges, RS485 isolation power
and other circuits may be DNP by profile.

The maximum-feature board is planned around an ESP32-S3-WROOM-1 no-PSRAM
module, the existing DX-LR30 LoRa module, two independent latching-valve
drivers, up to four selected-at-a-time protected RS485 branches, a local 3.3 V
I2C connector, controlled 5 V output and controlled protected battery-voltage
output. The production pressure sensors are planned for RS485, but their exact
model and register map remain unresolved; the live prototype still uses I2C.

The board accepts only low-voltage DC. Mains installations require an external
certified isolated 230 VAC to 12 VDC supply; a rectifier alone is not a safe
step-down supply. Solar/lead-acid builds also use an external lead-acid charge
controller. This PCB provides input/load protection and staged low-voltage
behavior, not lead-acid charging. The user's approximately 11.5 V proposal is
an initial actuation-inhibit threshold to validate, not a released whole-board
hard cutoff. The planned policy warns first, inhibits new OPEN operations,
reserves energy for CLOSE/status, and uses a lower hardware cutoff with
hysteresis.

Four RS485 branches must not have push-pull receive outputs tied directly
together. The production baseline selects one branch/transceiver and its
field-power domain at a time. Each port must explicitly declare full isolation
(including the power return) or signal-only isolation. A common battery return
defeats a claim of full galvanic isolation.

The short decision record is in
`UNIVERSAL_12V_CONTROLLER_PCB_PRELIMINARY_SPEC.md`. The detailed architecture,
provisional ESP32-S3 pin map, safety boundaries, test plan and open release
items are in `UNIVERSAL_12V_CONTROLLER_PCB_TECHNICAL_SPEC_DRAFT.md`.

### Explicit production pinout review, 2026-09-04

The two detailed specifications now contain complete GPIO-to-module-pad tables:
soil Draft 0.5 / C6-P1 in Section 11, universal Draft 0.2 / S3-P2 in Section 14.
Standalone Russian review copies are `SOIL_NODE_ESP32C6_PINOUT_RU.md` and
`UNIVERSAL_12V_ESP32S3_PINOUT_RU.md`.

C6 keeps its existing proposed GPIO allocation. GPIO12/13 use UART1 only after
disabling the default USB function; BOTH UART paths require powered-off
isolation. GPIO6/7 reuse pad-JTAG functions, so pad JTAG must not be enabled.

S3-P2 supersedes the earlier production proposal. GPIO39-42 are now debug-only
JTAG pads. Valve 1 PH/EN/power request use GPIO5/6/35, valve 2 GPIO38/47/36.
GPIO37 is the expander interrupt and GPIO48 the power-fault input. GPIO35-37
require the exact no-PSRAM N8 module. Slow enables, both nSLEEP permissions,
faults and four state inputs are assigned to a proposed TCA6424A (0x22).
Its battery-divider enable is P13; the ADC remains GPIO1. Output latches must
be cleared before changing expander directions. Hardware reset/interlocks
remain mandatory; an I2C error must not leave a valve energized.

The expander inputs are not lossless pulse counters and GPIO37 cannot provide
RTC deep-sleep wake. Asynchronous wake/pulse counting requires another review.
These are schematic proposals; no live prototype GPIOs or firmware targets
were changed by the documentation update.

### Contractor-facing technical specifications version 1.0 (superseded)

The user explicitly requires ENGLISH contractor-facing documentation. On
2026-09-04, the concise specifications were translated in full, retaining the
complete C6-P1 and S3-P2 pin maps. The following English documents are the
version 1.0 contractor-facing copies, without draft labels:

- `AmudarIO_Soil_Node_Technical_Specification_v1_0_EN.md`;
- `AmudarIO_Universal_12V_Controller_Technical_Specification_v1_0_EN.md`.

Editable DOCX copies are in `output/documents/`; matching PDFs are in
`output/pdf/`. The delivery is one ZIP containing the two English DOCX files
and their matching English PDFs, also saved in the user's Downloads folder.
Earlier Russian copies and detailed reviews are retained as background only;
do not send them to the contractor or substitute them for the English copies.
Version 1.0 refers to the requirements document, not measured PCB validation or
approval of unselected load ratings. Each specification retains a concise list
of cell/panel/sensor/valve data and calculated values to confirm before
fabrication. The live prototype firmware and its pinout remain unchanged.

### Contractor scope: PCB design only, version 1.1 (superseded)

The user clarified that this contractor designs the electrical schematic and
PCB only. Firmware, command protocols, cloud integration, enclosure design,
manufacturing, assembly, commissioning and physical product testing are not
part of the assignment. The current English contractor-facing sources are:

- `AmudarIO_Soil_Node_PCB_Design_Requirements_v1_1_EN.md`;
- `AmudarIO_Universal_12V_PCB_Design_Requirements_v1_1_EN.md`.

Version 1.1 retains the complete C6-P1 and S3-P2 GPIO/module-pad maps, the
TCA6424A port map, electrical defaults, power/interface protection and PCB
layout requirements. Application behavior, command/register descriptions and
software acceptance tasks are omitted. Pin state and leakage constraints are
electrical design requirements, not firmware work assigned to the contractor.
Assembly drawings, pick-and-place data and a PCB-only STEP model are CAD
deliverables; they do not assign board assembly or enclosure development.

The matching English DOCX and visually checked PDFs are in `output/documents/`
and `output/pdf/`. The four-file archive is saved in Downloads as
`AmudarIO_PCB_Design_Only_EN_v1_1.zip`. Version 1.1 supersedes version 1.0 for
contractor delivery. Older specifications and project firmware references are
preserved as internal background. No live firmware or GPIO assignments were
changed by this scope revision. Unselected load ratings still require customer
inputs before design release; document completion is not PCB validation.

### Latest hardware correction: mandatory MPPT and soil GNSS, version 1.2

The user explicitly requires MPPT on solar-powered nodes and GPS location
acquisition on every soil node. This supersedes every earlier GNSS exclusion
and the non-MPPT LTC4079 baseline. A once-daily location update was mentioned
only as an example; no interval has been fixed. Firmware changes are outside
the PCB contractor assignment and were not implemented in this revision.

Current contractor documents:

- `AmudarIO_Soil_Node_PCB_Design_Requirements_v1_2_EN.md`;
- `AmudarIO_Universal_12V_PCB_Design_Requirements_v1_2_EN.md`.

The soil charger baseline is LTC4121IUD-4.2#PBF, autonomous fractional-Voc MPPT
with a periodically sampled panel voltage, 4.2 V charging and cell-contact NTC.
It is not a fixed-Vmp regulator or a global power-curve sweep. The MPPT divider
must be populated and configured for the actual panel; RUN/MPPT sensing and
reverse blocking must avoid dark-current paths. Charging/MPPT remain available
with SYSTEM OFF and without MCU operation. The nominal 6 V panel still needs
Voc/Vmp/current data; maximum charge current remains 250 mA unless reviewed.

The soil GNSS baseline is u-blox MAX-M10S-00B with a separate active L1 antenna,
switched 3.3 V VCC/V_IO and switched antenna bias. Leave V_BCKP open and fit no
backup battery; cold starts are accepted. UART1 is shared sequentially with
automatic-direction RS485 using a power-gated TMUX1574PW. Its two signal paths
must disconnect before selected-domain power loss and remain isolated during
startup/selection changes. No simultaneous soil/GNSS UART use is provided.

Soil map C6-P2 keeps ESP32-C6-MINI-1-H4, every GPIO/module-pad number, all LoRa
signals and UART0 programming pins. GPIO6/pad15 changes from the extra wake
button to GNSS_SEL. GPIO2/pad5 is PERIPH_PWR_EN: LOW disables soil, GNSS and
the UART mux; HIGH powers only the branch selected by GPIO6 (0 soil, 1 GNSS).
GPIO12/pad17 and GPIO13/pad18 are renamed PERIPH_UART_TX/RX. GPIO1 remains the
independent battery-divider enable. The extra service-wake button is omitted;
BOOT and RESET remain. GPIO6 reset-time pull-up must not activate a load:
GPIO2 and reset/brownout hardware independently inhibit all branches.
The standalone current pin map is `SOIL_NODE_ESP32C6_PINOUT_C6_P2_EN.md`.

The 25 uA whole-board sleep target remains; below 1 mA is the mandatory
inactive-state ceiling. GNSS, antenna, mux, soil and divider must be OFF.
This is not a measured PCB result or a guaranteed 25 uA worst-case sum; MPPT
charger and protection leakage require full temperature budgeting. Do not
remove required MPPT/GNSS functions to meet the target.

The universal 12 V board retains S3-P2 and its external lead-acid charger
architecture. Solar installations now explicitly require an external MPPT
controller, not PWM-only or fixed-voltage charging. No solar panel or mains
connects directly to that PCB. The selected external controller must match the
panel, lead-acid battery, charge/float limits and temperature requirements.
GNSS has not been added to the universal board.

English DOCX/PDF delivery: Downloads/AmudarIO_PCB_MPPT_GPS_EN_v1_2.zip (four
files). Version 1.2 supersedes earlier contractor packages. Live firmware and
assembled prototype pin assignments remain unchanged.
