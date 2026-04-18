#pragma once
#include <Arduino.h>
#include "PrintController.h"
#include "RS485Modbus.h"

/*
  Shared runtime setup for single-file RS485 sensor diagnostic examples.

  Include config.h before this header so PCB_* and RS485_* macros are available.
*/

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static void beginRS485SensorExample() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);

  rs485.setDebug(&printer);

#if defined(ARDUINO_ARCH_ESP32)
  rs485.begin(RS485Port,
              RS485_DEFAULT_BAUD,
              PCB_RS485_RX_PINS[RS485_PORT_INDEX_0],
              PCB_RS485_TX_PINS[RS485_PORT_INDEX_0],
              RS485_DEFAULT_SERIAL_CONFIG);
#else
  rs485.begin(Serial2, RS485_DEFAULT_BAUD, -1, -1, RS485_DEFAULT_SERIAL_CONFIG);
#endif

  rs485.setDirectionControl(PCB_RS485_DE_PINS[RS485_PORT_INDEX_0],
                            PCB_RS485_DE_ACTIVE_HIGH[RS485_PORT_INDEX_0]);
}
