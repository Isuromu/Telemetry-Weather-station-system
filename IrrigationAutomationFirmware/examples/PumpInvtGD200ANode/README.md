# INVT GD200A staging example

This is a safe placeholder for commissioning the `GD200A-022G/030P-4` before
it replaces the temporary DELIXI drive in `PumpControl`.

The current program only prints the verified model-independent Modbus
addresses. It does not initialize RS-485, write VFD parameters, set frequency,
or issue run/stop commands.

Build it with:

```text
pio run -e invt_gd200a_staging
```

Before this becomes a live read-only diagnostic, answer these questions:

- Which ESP32 board and RS-485 converter will be used on the pump node?
- Does the converter use automatic direction, or does it need a DE/RE GPIO?
- What are the actual VFD address, baud rate, parity, and stop-bit settings?
- Is the RS-485 terminal marked `485+`/`485-`, `A`/`B`, or another convention?
- Is signal ground connected between the controller and VFD?
- Can the first bench session be performed with the motor mechanically safe
  and run commands disabled?

After those facts are known, the next step is to add read-only commands for
device code `0x2103`, state `0x2100`, fault `0x2102`, and monitoring registers
`0x3000` through `0x3005`.
