# Open Engineering Items

These are unresolved facts, not invitations to invent values.

## PressureControlValve

1. Confirm which polarity physically means OPEN.
2. Confirm which polarity physically means CLOSE.
3. Measure/confirm required pulse duration.
4. Measure pulse current from the actual 12 V supply.
5. Confirm whether repeated same-direction pulses are harmless for this exact solenoid.
6. Confirm GPIO27 active level against the physical BJT/P-MOSFET circuit.
7. Determine hydraulic settling time before post-command pressure verification.
8. Define when pressure readings are sufficient to infer PCV state.
9. Decide whether last commanded state should be stored in NVS.

## Pressure sensors

1. Confirm exact XDB401/S1204 datasheet.
2. Validate I2C address(es).
3. Validate register map.
4. Validate pressure scale and signed representation.
5. Validate temperature formula.
6. Calibrate against a known pressure reference if required.

## LoRa

1. Inspect repository and gateway configuration.
2. Confirm raw LoRa versus LoRaWAN for this firmware.
3. Confirm radio frequency / regional configuration actually used in deployment.
4. Confirm whether RXEN/TXEN are controlled directly by the current SX1262 library.
5. Define command transaction IDs and duplicate-command behavior.

## I2C architecture

1. Prototype Rev A stays dual-bus.
2. Choose exact multiplexer part for Rev B, likely TCA9548A or equivalent.
3. Assign upstream/downstream mux channels.
4. Release GPIO13/GPIO4 only after hardware actually migrates.

## RS485

1. Confirm whether the future module is the same auto-direction RS485 board.
2. Reserve GPIO13 TX and GPIO34 RX after I2C migration.
3. Reuse/refactor the existing `RS485ModBus` library.
4. Integrate PumpVfd first or the motorized valve first based on the actual deployment sequence.

## Battery

1. Verify actual divider resistor values.
2. Calibrate ADC against a multimeter.
3. Confirm the physical over-discharge thresholds of the assembled circuit.
4. Decide whether firmware should only report low battery or also refuse valve actuation below a safety threshold.
