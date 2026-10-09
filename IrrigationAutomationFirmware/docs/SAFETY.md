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

## The free stop (`estop`) is a damaging stop, not a better one

Two different stops exist, and the difference matters:

| command | DELIXI write | behaviour |
|---|---|---|
| `pump stop` (fPort 50, op 01) | `0x0006` deceleration stop | the drive ramps the motor down over its configured decel time |
| `pump estop` (fPort 50, op 02) | `0x0005` free stop | the drive cuts its output; motor and impeller coast on their own inertia |

- A free stop removes the only control over the flow rate. Against a static head
  the column decelerates on its own and can reverse before the impeller stops, so
  the non-return valve closes onto a reversing flow — a pressure surge in the
  pipe, valve and casing (water hammer). A deceleration stop ramps the flow down
  under control instead.
- Repeated free stops load the check valve and can cavitate the pump on a long
  drop. Treat a free stop as an event to go and inspect, not as routine shutdown.
- It is not regeneration that makes this dangerous: coasting applies no braking
  torque, so nothing is pushed back into the VFD DC bus. A *fast deceleration*
  stop is what regenerates and can trip the drive on overvoltage — that is a
  drive decel-time setting, a separate concern from this command.
- It is also still a software command over LoRaWAN, not a physical emergency-stop
  circuit (see Functional limits), so it can be lost like any other downlink.

**Policy.** The escalation for an unanswered pump stop must be: resend the
**deceleration stop** while the pump is still reporting, and reach for the free
stop only if that also goes unanswered. One lost downlink is not evidence that
the pump is out of control. Do not raise `pumpStopTimeoutSec` to avoid the
escalation — the missing step is the repeat, not a longer wait.

Observed 2026-10-09: one lost `stop` downlink took the sequencer straight to the
free stop at the 150 s deadline. The pump had been reporting normally throughout
(11 uplinks in that window), so a free stop was issued on a healthy pump for the
sole reason that one frame did not land.
