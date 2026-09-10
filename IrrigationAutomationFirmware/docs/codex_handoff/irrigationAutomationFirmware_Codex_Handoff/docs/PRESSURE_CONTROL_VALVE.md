# Pressure-Control Valve (PCV) Control

## Why the name matters

The project contains more than one valve-like device.

The current hydraulic unit is not merely a generic on/off valve. It is a pressure-control / pressure-reducing valve with pressure sensors before and after it.

Canonical code name:

`PressureControlValve`

Short form:

`PCV`

The 12 V actuator should be called:

`PressureControlValveSolenoid`

The separate FC11C RS485 motorized actuator should never be called `PressureControlValve` unless the hardware architecture is intentionally merged later.

## Latching behavior

The installed/new solenoid is latching.

Therefore:

- OPEN = a short pulse with one polarity;
- CLOSE = a short pulse with the opposite polarity;
- holding 12 V continuously is unnecessary and undesirable;
- cutting power is not a CLOSE command.

## Power-up and pulse sequencing

Recommended control sequence:

```text
1. L298N IN1 = LOW
2. L298N IN2 = LOW
3. Switch L298N power ON through GPIO27
4. Wait POWER_SETTLE_MS
5. Apply OPEN or CLOSE polarity
6. Hold for SOLENOID_PULSE_MS
7. IN1 = LOW
8. IN2 = LOW
9. Wait POST_PULSE_MS
10. Switch L298N power OFF
11. Wait HYDRAULIC_SETTLE_MS before pressure-based verification
```

Starting values for bench testing can be centralized as:

- `POWER_SETTLE_MS = 20`
- `SOLENOID_PULSE_MS = 250`
- `POST_PULSE_MS = 20`
- `HYDRAULIC_SETTLE_MS = 1000`

These are engineering starting values, not yet validated final values for the exact solenoid.

## Polarity mapping

Do not hard-code the semantic mapping in multiple places.

Use a central configuration:

```cpp
OPEN:  IN1 = HIGH, IN2 = LOW
CLOSE: IN1 = LOW,  IN2 = HIGH
```

If bench testing proves the physical action is reversed, swap the mapping in one configuration location only.

## Should firmware check state before pulsing?

Yes, but only with a careful state model.

There are three different concepts:

1. **Last commanded state**
   - what the ESP32 most recently requested;
   - can be stored in RAM/NVS;
   - not guaranteed to equal physical reality.

2. **Electrical solenoid state**
   - a latching solenoid has no inherent position-feedback wire in the current design;
   - the controller cannot directly read this state.

3. **Hydraulic inferred state**
   - estimated from upstream/downstream pressures;
   - valid only when the system is pressurized and the hydraulic conditions make the inference meaningful.

Therefore the controller should not blindly do:

```text
read pressures -> conclude OPEN/CLOSED -> skip pulse
```

That can fail when:

- pump is off;
- there is no flow;
- line is depressurized;
- pressures have equalized;
- downstream load changes independently.

A better command policy:

```text
Receive desired state.

Read current telemetry.

If reliable hydraulic verification is available
AND verified state already equals desired state:
    do not pulse.
Else:
    pulse desired polarity.
    wait for hydraulic settling.
    read pressures again.
    report commanded state and, if possible, verification result.
```

On boot, use:

`Unknown`

unless there is a reliable external mechanism to determine the actual state.

## State model

Recommended:

```cpp
enum class PressureControlValveState : uint8_t {
    Unknown,
    Open,
    Closed
};
```

Optionally separate:

```cpp
enum class VerificationState : uint8_t {
    NotAvailable,
    Unverified,
    Verified,
    Failed
};
```

This prevents a stored software variable from masquerading as physical feedback.

## Power-gate rationale

Switching the L298N power rail off when idle:

- eliminates unnecessary quiescent consumption;
- prevents the latching coil from remaining energized;
- reduces heat;
- makes the end-node friendlier to solar/battery operation.

Always drive IN1/IN2 LOW before enabling or disabling the L298N rail to reduce back-powering and undefined states.
