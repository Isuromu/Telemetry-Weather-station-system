# INVT GD200A library staging area

This directory is reserved for the Modbus RTU driver for the
`GD200A-022G/030P-4`. The verified register and command constants are in
`src/InvtGD200AProtocol.h`.

The motor-control class is deliberately not implemented yet. The active pump
firmware still uses `DelixiCDIE100`, so adding this directory cannot start or
reconfigure the new VFD.

## Questions to answer before implementing control

1. Is the exact nameplate model `GD200A-022G/030P-4`, including the `-4`
   three-phase 380--440 V voltage class?
2. Will the installation use G mode (22 kW, 45 A output) or P mode
   (30 kW, 60 A output)? What is the actual value of `P00.17`?
3. What motor is connected? Record manufacturer, model, rated power, voltage,
   current, frequency, speed, and permitted minimum operating frequency.
4. What are the commissioned values of `P14.00` through `P14.06`?
5. What are the commissioned values of `P00.00` through `P00.07`, especially
   the run-command and frequency-command sources?
6. Which stop behavior is required for normal stop and for the software
   emergency command: ramp stop or coast-to-stop?
7. What acceleration and deceleration times are safe for the pump and its
   hydraulic system?
8. Should firmware ever write motor/VFD parameters, or should it provide
   read-only validation plus run/frequency commands?
9. Which GD200A manual revision matches the physical unit?

Answers should be recorded in `docs/INVT_GD200A.md` before the driver is wired
into `PumpController`.
