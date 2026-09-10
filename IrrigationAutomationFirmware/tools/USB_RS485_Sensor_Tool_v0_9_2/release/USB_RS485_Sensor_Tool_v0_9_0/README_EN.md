# USB-RS485 Sensor Tool v0.9.0

Standalone utility for USB-to-RS485 Modbus sensors and EPEVER Tracer-AN G3 configuration.

For the standalone build, run `USB_RS485_Sensor_Tool.exe` or
`START_USB_RS485_SENSOR_TOOL.bat`. To run from source, execute
`INSTALL_REQUIREMENTS.bat` once and then `START_USB_RS485_SENSOR_TOOL.bat`.

## TUF-2000M + TS-2

Select `TUF-2000M + TS-2 Ultrasonic Flow Meter`, use address 1 and profile
baud 9600, then click `Send read request`. One action reads flow/velocity,
M08 error bits, M90 signal diagnostics, M91 time ratio, inner pipe diameter,
and M46 device address. Every complete TX/RX frame is shown.

The REAL4 word order is not assumed. Both interpretations are displayed for
comparison with M01/M91; the correct inner-diameter interpretation should be
approximately 57 mm for the commissioned pipe.

## EPEVER Config

The EPEVER Config tab can read and write selected Tracer-AN G3 settings using Modbus FC10:

- battery type = User;
- battery capacity Ah;
- voltage block `0x9003..0x900E` as one group;
- rated voltage level `0x9067`;
- max charging current `0x90BF`;
- EPEVER ID/address change using the captured custom service command `0x45`.

## EPEVER address change

v0.8.0 adds EPEVER ID change using the proprietary/custom service command `0x45`. This is not normal Modbus FC06/FC10.

Commands:

```text
Find/read ID: F8 45 00 01 01 F8 + CRC
Set ID:       F8 45 00 01 01 NN + CRC
Example 0x60: F8 45 00 01 01 60 88 14
```

Use this only when exactly one EPEVER controller is connected to the USB-RS485 adapter. After writing, the app verifies the new and old addresses using a normal FC04 battery-voltage read.

Do not use `0x9020` as an address register. In the uploaded G3 protocol it is Turn-Off Voltage, not ID/address.


## v0.8.0 notes

The EPEVER tab is now split into settings on the left and a permanently visible TX/RX log on the right. False voltage-order blocking for config.h values has been fixed: 0x900A and 0x900B are treated as separate chains.
