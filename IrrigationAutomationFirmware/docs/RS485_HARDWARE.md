# RS-485 hardware

This file describes the current single-converter prototype. The planned
universal production PCB keeps automatic direction but adds selected-at-a-time
protected field branches and an explicit isolation/power-return definition.
See `UNIVERSAL_12V_CONTROLLER_PCB_TECHNICAL_SPEC_DRAFT.md`, Section 11. Do not
tie multiple push-pull receiver outputs directly together as a passive star.

## Prototype converter

The supplied reverse-engineered module uses a 74HC14-based automatic direction
circuit. UART TXD/RXD are connected without a DE/RE GPIO. The board includes
line biasing, TVS protection, polyfuses, and a shown 120 ohm termination
resistor. It has no digital isolator and no isolated DC/DC converter.

The active board profile therefore selects:

```text
Rs485DirectionMode::Automatic
direction pin = -1
```

`Rs485ModBus` also retains manual direction mode for converters that expose a
DE/RE control input.

## Wiring

```text
ESP32 GPIO17 TX -> converter TXD
ESP32 GPIO16 RX <- converter RXD
converter A+    -> DELIXI SG+
converter B-    -> DELIXI SG-
converter GND   -> controller reference as required by the module design
```

Verify polarity from actual labels before energizing. If communication is
silent, do not swap wires while equipment is energized.

## Termination and installation

Use 120 ohm termination only at the two physical ends of an RS-485 trunk. The
supplied module schematic shows a fitted termination; account for it when more
devices are added. Do not install termination at every node.

Use shielded twisted pair, short stubs, a suitable common reference, and route
the cable away from VFD input/output conductors. For production, use a
galvanically isolated RS-485 transceiver and installation-grade surge/EMC
protection selected for the site.

## TUF-2000M valve-node integration

The repository contains a `FlowMeter` interface and a `Tuf2000mFlowMeter`
driver that receives the existing generic `Rs485ModBus` transport. The driver
implements the manual's function-03 reads for flow rate, velocity, error bits,
and accumulated volume. UART2 GPIO16/GPIO17 is assigned to the node's
auto-direction converter. The user has confirmed that the assembled ESP32
reads live flow and velocity successfully. Remaining items are:

- power and wake/stabilization behavior;
- final production reference/isolation and termination arrangement.

M46 address 1, M62 9600 8N1, and M63 MODBUS_RTU are commissioned. The normal
`flow` command uses the hardware-confirmed `LOW_WORD_FIRST` REAL4 decoder and
performs one measurement read plus one error-register read; it never enables
continuous polling. The `flow probe` command remains available and prints the
raw response plus both word-order interpretations for diagnostics.
`flow total` performs one additional read of REG0113-REG0118. Its reset is a
local NVS baseline operation and sends no write to the meter.
