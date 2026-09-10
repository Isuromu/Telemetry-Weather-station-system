# Safety

## Electrical hazards

The CDI-E100G2R2T4B is a 380 V three-phase VFD with an IP20 enclosure. It is not
waterproof and exposes hazardous energy. Only qualified personnel may install,
commission, or maintain it.

- Isolate and lock out incoming power before work.
- Wait the discharge interval required by the DELIXI manual and verify absence
  of hazardous voltage.
- Connect mains only to R/S/T. Never connect mains to U/V/W.
- Connect the motor only to U/V/W and protective earth.
- Do not switch or disconnect the motor output while the VFD is running.
- Use correct overcurrent protection, cable ratings, enclosure, cooling, and
  grounding for the installation.
- Keep control/communication wiring separate from power and motor wiring.
- A long motor cable may require an output reactor according to the VFD manual
  and installation conditions.

## Functional limits

- Firmware is not a safety-rated control system.
- `pump estop` is a communication command, not a physical emergency stop.
- Loss of MCU power, software failure, cable damage, or EMI can prevent a
  command from reaching the VFD.
- Provide a hardwired emergency-stop and isolation arrangement appropriate to
  the risk assessment and local rules.
- The current prototype RS-485 converter is not galvanically isolated. Use an
  isolated interface for a robust production installation near a VFD/motor.
- Enable and test a non-zero VFD communication timeout after initial bring-up.

## Pump hazards

- Never run the pump dry.
- Confirm priming, valve state, permitted pressure, and free flow path.
- Verify rotation at low frequency before increasing speed.
- Do not exceed 50 Hz or the 3.3 A motor rating.
- Watch for leaks, pipe movement, cavitation, vibration, overheating, and
  abnormal noise.
- Maximum head and maximum flow are not simultaneous operating values.
