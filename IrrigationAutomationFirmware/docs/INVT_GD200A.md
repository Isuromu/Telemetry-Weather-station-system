# INVT GD200A pump VFD

## Project status and hardware confirmation

The planned production pump inverter is `GD200A-022G/030P-4`. The existing
`DelixiCDIE100` integration remains active temporarily. The INVT library and
example are staging-only and cannot control hardware yet.
The planned model and code constants do not establish hardware commissioning;
installed-unit identity and settings remain subject to the checks below.

## Source selection

Use [manual 2](references/INVT_GD200A/invt-gd200a-user-manual2.pdf):
*Goodrive200A Series VFD*, English, 201908 (V2.4), document 66001-00342
(edition information on PDF p. 179). Page references here are 1-based PDF pages.

[Manual 1](references/INVT_GD200A/invt-gd200a-user-manual1.pdf) has a GD200A
filename but its preface (PDF p. 2) identifies **Goodrive200**. Keep it as a
related reference; do not assume its parameter definitions apply to GD200A.

## Confirmed manual facts

- Physical protocol: two-wire RS-485, Modbus RTU.
- Control command: `0x2000`.
- Frequency command: `0x2001`, scaled in `0.01 Hz`.
- Run state, fault, and device code: `0x2100`, `0x2102`, and `0x2103`.
- Expected GD200A device code: `0x0107`.
- Monitoring registers start at `0x3000`.
- `P00.01` selects the run-command channel, `P00.02` selects Modbus, and the
  selected frequency source must be configured for Modbus control.
- `P14.00` through `P14.06` contain serial communication settings and behavior.
- The model is rated 22 kW/45 A in G mode or 30 kW/60 A in P mode.

Source locations in manual 2: chapter 9, PDF pp. 143-146 for the control,
status, identification and monitoring tables; pp. 38-39 for command/frequency
sources; pp. 96-98 for P14 communication settings; p. 15 for model ratings.

## Open commissioning questions

- Exact nameplate photograph and manual revision?
- G or P operating mode, and actual `P00.17` value?
- Complete connected-motor nameplate data?
- Actual `P00.00`--`P00.07` values?
- Actual `P14.00`--`P14.06` values?
- Required normal-stop and software-emergency-stop behavior?
- Approved minimum/maximum frequency and ramp times for this pump?
- Should configuration application remain available, or should production
  firmware be read-only except for control and frequency commands?

Do not replace the active pump driver or enable configuration writes until
these items are resolved.
