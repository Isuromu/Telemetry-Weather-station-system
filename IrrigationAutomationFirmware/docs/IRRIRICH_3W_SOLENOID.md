# IrriRich 3-way solenoid reference

## Source identity and limitation

Supplied file: [irririch_3w_latch_solenoid.pdf](references/Irririch_3w_latch_solenoid/irririch_3w_latch_solenoid.pdf).
It is an English manufacturer product-page print dated 2026-09-29, not a
revision-controlled installation manual. The file contains two PDF pages,
with printed footers `1/4` and `2/4`; the dimensions image is partly clipped.
Page references below mean the two actual PDF pages.

**The filename and main content do not match:** both the title and printed
source URL identify **Regular Model 3-Way Solenoid Actuator**. "LATCH 3-Way
Solenoid Actuator LATCH" appears as a previous/related product, not as the
subject of the specification table. Preserve this source as supplied, but do
not use its regular-coil ratings to validate a latching coil.

The title identifies `3W` with the **3-way** product family. It does not
establish three electrical wires or a 3 W power rating.

## Source facts: regular-model product page

PDF p. 1 lists:

- Separate 24 VAC, 24 VDC and 12 VDC variants, with allowed voltage deviation
  of +/-10%; this is not a universal-input specification for one coil.
- A one-minute pressure test at 0-10 bar with no leakage. The page does not
  establish a complete continuous operating-pressure specification.
- A 3/4-inch, 20 UNEF male base connection and IP66 protection.
- Maximum ambient temperature 60 degrees C and maximum fluid temperature
  80 degrees C; NBR or EPDM seals.

The technical table continues on PDF p. 2:

| Regular-model designation | Listed wire color | Inrush current | Holding current |
|---|---|---:|---:|
| `3W-D-NO-24VDC` | Black | 0.12 A | 0.12 A |
| `3W-D-NO-12VDC` | Blue | 0.28 A | 0.28 A |

These are manufacturer-listed regular-model values, not measurements or
latching pulse-current ratings. The table also lists 24 VAC variants; do not
mix their ratings with the DC variants. Wire-color entries do not specify
OPEN/CLOSE polarity. A related-product photograph does not supply the missing
latching model's electrical specification.

## Project implementation and configuration

The [pressure-node implementation](PRESSURE_CONTROL_NODE.md) operates a latching
solenoid using opposite-polarity L298N pulses and removes bridge power between
commands. Its [configuration](../examples/PressureControlNode/include/PressureNodeConfig.h)
currently specifies a 250 ms pulse, 20 ms power-settle time and 20 ms post-pulse
time. The source explicitly labels the timings as bench starting values and
the OPEN/CLOSE polarity mapping as assumed.

The new PDF does **not** validate those values or authorize changing the driver
to sustained coil energization. The project requirement for a latching valve
remains in effect. The adjacent XDB401 pressure-sensor documentation concerns
a different device; this solenoid source resolves no XDB401 register or scaling
questions.

## Hardware-confirmed observations

Existing project context records a latching solenoid on the valve node.
No installed IrriRich model number, coil-label record, measured pulse current,
minimum reliable pulse duration or confirmed wire-to-polarity mapping was
identified in the material reviewed for this addition. Manufacturer product
photographs are not observations of the installed valve.

## Unresolved items

- Identify the exact installed coil/model and obtain its matching **latching**
  datasheet, including pulse voltage, current/resistance, pulse-duration limits
  and permissible repetition rate.
- Confirm electrical lead identification and which polarity produces each
  hydraulic state. Three-way port routing and manual-override position also
  need the model-specific installation information.
- Measure actuation reliability, coil voltage/current and driver voltage drop
  on the assembled node before changing the current timing or polarity values.
- Obtain the complete dimensional/connection drawing before checking fit or
  selecting a replacement. This partial regular-model print is insufficient.
