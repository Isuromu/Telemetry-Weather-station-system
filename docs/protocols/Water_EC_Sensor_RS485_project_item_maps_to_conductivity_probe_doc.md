# Water EC Sensor (RS485)

This project item maps to the JXEC-T water conductivity controller + probe protocol.

See:

```text
JXEC_T_Series_Conductivity_Probe_Water_EC_TDS_Temp_RS485.md
```

Key implementation notes:
- Use the RS485 transmitter/controller assembly, not a bare metal probe alone.
- Default address: `0x01`
- Default serial: `9600 8N1`
- Combined read request: `01 03 00 01 00 03 54 0B`
- Registers:
  - `0x0001`: water temperature, raw / 10.0 = C
  - `0x0002`: conductivity high word
  - `0x0003`: conductivity low word
- K=1 example scaling: `conductivity_uS_cm = conductivity_raw / 100.0`

The conductivity scale depends on the installed probe/controller range and cell constant. Keep the scale configurable during hardware bring-up.
