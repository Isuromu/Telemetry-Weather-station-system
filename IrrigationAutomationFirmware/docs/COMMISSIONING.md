# Pump commissioning

## Before applying power

1. Commissioning must be performed by personnel qualified for 380 V VFD and
   motor systems.
2. Lock out power before touching wiring and wait the manufacturer-specified
   discharge time.
3. Confirm protective earth, input R/S/T, output U/V/W, motor nameplate, and
   terminal tightness.
4. Confirm RS-485 polarity: converter A+ to VFD SG+, B- to VFD SG-.
5. Route shielded twisted-pair communication cable away from input and motor
   power cables.
6. Prime the centrifugal pump. Never test it dry.
7. Put valves and the hydraulic circuit into a safe test condition.
8. Keep a physical stop or isolator available.

## Communication-only check

Power the control system and VFD, but do not request a run.

```text
vfd ping
vfd info
vfd read 0xB000
vfd read 0xB001
vfd config check
```

Review every warning and error. The check does not change the VFD. If the VFD
is still in keyboard mode or uses another frequency source, decide whether to
commission those values manually from the keypad or use the explicit validated
apply command while the VFD is stopped:

```text
vfd config apply CONFIRM
```

Do not use parameter identification as part of this initial procedure.

## First rotation test

1. Make the work area safe and confirm the pump is filled with water.
2. Set `pump freq 10`.
3. Send `pump start` briefly.
4. Verify forward rotation and immediately send `pump stop` if it is wrong.
5. Correct rotation by qualified rewiring with power isolated; do not use the
   reverse command as a pump operating mode.
6. Inspect leaks, pressure, flow, vibration, sound, and motor current.
7. Increase gradually through tested values such as 20, 30, 40, and 50 Hz.
8. Use `pump telemetry` at each point and keep current within the 3.3 A motor
   rating.
9. Stop and review `pump fault`.

The 10 Hz software limit is for controlled testing, not a guarantee of safe
continuous operation.

## Deployment follow-up

After communication is stable, choose and validate a non-zero P4.1.04
communication timeout. Test the complete loss-of-communication behavior under
controlled conditions. A physical emergency-stop circuit remains required by
the installation's risk assessment.
