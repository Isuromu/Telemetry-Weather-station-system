# SolarController_EP_LS1024B

Driver for the **EPEVER LandStar LS1024B** solar charge controller over RS-485
Modbus RTU. It reads live charge data, reads the controller's settings, writes the
battery charge setpoints, and can change the controller's Modbus address.

```cpp
#include <SolarControllerEpLs1024B.h>

RS485Bus transport;
SolarControllerEpLs1024B controller(transport, {
    0x60,   // slave address
    300,    // response timeout, ms
    false,  // debug
});         // + expected battery type (User) and rated voltage level (12 V)

transport.setDirectionMode(Rs485DirectionMode::Automatic);
transport.begin(Serial2, 115200, 16, 17, SERIAL_8N1);
controller.begin();

const SolarControllerMeasurements reading = controller.readMeasurements();
```

## The consequence you need to know

**Battery type, capacity, temperature compensation, rated voltage level, and
maximum charging current are read-only in this driver. They must be set on the
controller itself** — through its own display, or the EPEVER configuration tool.
The firmware reads them and refuses to write the charge setpoints unless the
controller already reports a user-defined battery type (`0x9000 == 0`) and a 12 V
rated voltage (`0x9067 == 1`).

Only the twelve mutually constrained charge setpoints at `0x9003..0x900E` are
ever written, as one FC10 block request, and only after the local ordering rule
passes. Every write is read back before it is reported as applied.

## Device facts

| Item | Value |
| --- | --- |
| Commissioned slave address | `0x60` (96) |
| Framing | 115200 8N1, Modbus RTU |
| Real-time values | FC04 input registers, 0.01 V / 0.01 A / signed 0.01 C |
| Settings | FC03 read, FC10 block write of `0x9003..0x900E` |
| Address change | proprietary service command `0x45`, broadcast, **not Modbus** |

The controller powers itself from the battery. Its **LOAD output is
battery-tracking, not a regulated 14.4 V supply** — anything connected to LOAD
must tolerate the full charge voltage, or be fed through a DC-DC converter. No
firmware on this node can clamp that.

## Write gating

Writes are compiled out unless the build defines
`SOLAR_CONTROLLER_WRITES_ENABLED=1`, and the bench console additionally requires
an explicit `confirm` word. A build without the flag reports `WRITES_DISABLED`
and puts nothing on the wire.

## Status

Bench-only and **not verified against hardware**: the real-time registers for
battery voltage, battery temperature, load current, and battery status are proven
on the installed controller, but the settings map and the FC10 write path come
from the configuration sketch and are flagged as unconfirmed in the code. The
`pcv_solar_test` target is the experiment that settles it.

- Register provenance and the bench procedure: `docs/EPEVER_LS1024B.md`
- Decision history and open items: `CHANGELOG.md`
- Cross-checking from the PC side: `tools/USB_RS485_Sensor_Tool_v0_9_2/`
