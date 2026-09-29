# Epever LS1024B solar charge controller

## Source and scope

Source: EPEVER [LS-B series manual V3.3](references/Epever_LS1024B/Epever_LS1024B_manual.pdf),
English. The file has **2 PDF pages**, each containing two printed pages:
PDF p. 1 = printed pp. 1-2; PDF p. 2 = printed pp. 3-4.

## Manual facts

Section 1 (PDF p. 1) identifies LS-B as a **PWM, common-positive** controller.
It is not an MPPT controller. Charging stages are bulk, boost/equalize and
float; battery profiles are sealed, gel, flooded and user-defined.

Section 8 (PDF p. 2, printed p. 4), specifically the **LS1024B** column:

| Property | Manual value |
|---|---|
| Nominal system | 12/24 VDC, automatic selection |
| Rated charge / discharge current | 10 A / 10 A |
| Controller working voltage | 8-32 V |
| Maximum PV open-circuit voltage | 50 V |
| Self-consumption | <=8.4 mA at 12 V; <=7.8 mA at 24 V |
| Grounding | Common positive |
| Ambient temperature / enclosure | -35 to +50 degrees C / IP30 |

The 8-32 V controller working range and 50 V PV open-circuit limit are
different specifications; they are not interchangeable input ratings.

### Connections and load behavior

- Sections 2-3 (PDF p. 1, printed p. 1) identify separate battery, PV and
  load terminals, an RS485 port and a remote temperature-sensor port.
  The prescribed connection order is battery, load, PV; disconnect in reverse.
- Section 3 calls for battery-side overcurrent protection and identifies
  positive-ground construction. Do not assume all negative terminals are a
  common return or bridge them through another board without checking the circuit.
- Section 6 (PDF p. 2, printed p. 3) says low-voltage disconnection affects
  loads on the controller's load terminals. Battery-direct loads remain connected.
- Section 5 (printed pp. 2-3) lists manual control (default ON), light ON/OFF,
  light ON/OFF plus timer, and time control. Load behavior depends on the
  selected mode, not only battery voltage.

### Published voltage settings, not commissioned values

Section 5(1), PDF p. 1 / printed p. 2, gives values for a **12 V system at
25 degrees C**; it instructs doubling voltages for 24 V systems. For the
sealed/gel/flooded columns, low-voltage disconnect is 11.1 V and reconnect
is 12.6 V; float is 13.8 V. Boost is respectively 14.4 / 14.2 / 14.6 V.
The table is a manufacturer profile, not approval for the installed battery
or the project's actuation-inhibit thresholds. Read the full table, battery
requirements and actual configuration before changing charging settings.

### Communications coverage

Sections 1, 2 and 5 confirm RS485 with Modbus and describe PC/phone/MT50
configuration accessories. This supplied manual **does not provide the
register map, serial framing/baud, unit address or connector pinout**.
Do not invent those values or borrow an MPPT controller's map without a
model-applicable protocol reference. A connector drawing alone is not a pinout.

## Project implementation and requirements

No LS1024B driver or commissioned configuration was identified in the reviewed
firmware. Adding this manual is not evidence that the controller has been
selected, installed or tested on a particular node.

The [current project context](CURRENT_PROJECT_CONTEXT.md#latest-hardware-correction-mandatory-mppt-and-soil-gnss-version-12)
and [universal PCB requirements v1.2](AmudarIO_Universal_12V_PCB_Design_Requirements_v1_2_EN.md)
require an external **MPPT** controller for production solar/lead-acid builds.
LS1024B's documented PWM charging does not meet that requirement. Retain it
as a reference for applicable existing hardware; it does not supersede MPPT.
Its own consumption must also be counted separately from MCU/PCB sleep current.

## Hardware-confirmed observations

No installed-unit label, configuration readback, wiring record or measured
current was identified for LS1024B. Values above are manual facts only.

## Unresolved items

- Establish which existing node, if any, uses LS1024B and its exact revision.
- Record battery chemistry/capacity, panel ratings, actual charging profile,
  temperature-sensor arrangement and load mode.
- Establish whether each load is battery-direct or on the controller output,
  including the common-positive return and RS485 grounding arrangement.
- Obtain the applicable communication specification before implementing a driver.
- Confirm the selected MPPT model separately for production solar hardware.
