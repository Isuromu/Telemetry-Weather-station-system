# PumpControl example

This example is the first commissioning application for the configured
Grandfar 2CP50/160B pump and DELIXI CDI-E100 VFD.

Build it with:

```text
pio run -e pump_control_example
```

The example initializes Serial at 115200 baud and RS-485 at the CDI-E factory
setting of 9600 baud, 8N1, slave address 1. It checks communication and VFD
configuration, sends only a stop command during boot, and then waits for text
commands. It never starts the pump automatically.

Start with `help`, `vfd ping`, `vfd config check`, and `pump status`.
