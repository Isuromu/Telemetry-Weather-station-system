# Grandfar 2CP50/160B profile

## Nameplate

| Property | Value |
|---|---:|
| Motor type | three-phase asynchronous |
| Rated power | 1.5 kW / 2.0 HP |
| Rated voltage | 380 V |
| Rated current | 3.3 A |
| Rated frequency | 50 Hz |
| Rated speed | 2900 rpm |
| Duty | continuous |
| Protection | IP44 |
| Insulation | class B |
| Maximum flow | 167 L/min |
| Maximum head | 44 m |
| Suction depth | 9 m |
| Connections | 2 inch x 2 inch |

Maximum flow and maximum head are opposite ends of the pump curve and do not
occur simultaneously.

## Firmware limits

- Minimum command: 10 Hz
- Maximum command: 50 Hz
- Reverse: prohibited
- Initial acceleration/deceleration: 20 s / 20 s

The 10 Hz minimum is a project commissioning limit. It is not presented as a
manufacturer-approved continuous operating point. Hydraulic performance,
motor cooling, lubrication, vibration, and site tests may require a higher
continuous minimum.

The displayed RPM value is an estimate calculated from measured output
frequency and the 2900 rpm / 50 Hz nameplate ratio. It is not encoder feedback.
