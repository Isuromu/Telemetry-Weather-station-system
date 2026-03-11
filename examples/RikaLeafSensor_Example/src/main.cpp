#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaLeafSensor.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static RikaLeafSensor leaf(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_5_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Rika Leaf Sensor Simple Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Optional scan in setup()"), true);
  printer.println(F("- Then plain polling"), true);
  printer.println(F(""), true);
}

static void printReadResult(bool ok) {
  if (ok) {
    printer.print(F("[APP] Sensor ID: "), true);
    printer.print(leaf.getSensorId(), true, " | ");
    printer.print(F("Address: 0x"), true);
    printer.print((unsigned int)leaf.getAddress(), true, " | ", HEX);
    printer.print(F("Temperature: "), true);
    printer.print(leaf.leaf_temp, true, " C | ", 1);
    printer.print(F("Humidity: "), true);
    printer.print(leaf.leaf_humid, true, " %", 1);
    printer.println("", true);
  } else {
    printer.print(F("[APP] Read failed for sensor "), true);
    printer.print(leaf.getSensorId(), true, " | ");
    printer.print(F("Address: 0x"), true);
    printer.print((unsigned int)leaf.getAddress(), true, " | ", HEX);
    printer.print(F("Error count: "), true);
    printer.println((unsigned int)leaf.getConsecutiveErrors(), true);
  }
}

void setup() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);
  printBanner();

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

  if (DO_SCAN) {
    printer.println(F("[APP] Scan mode is enabled. Searching address..."), true);
    const uint8_t found = leaf.scanForAddress(1, 247, 120, 20);
    if (found != 0) {
      printer.print(F("[APP] Sensor found at address 0x"), true);
      printer.println((unsigned int)found, true, "", HEX);
    } else {
      printer.println(F("[APP] No sensor responded during scan."), true);
    }
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] New polling cycle"), true);
  printer.println(F("------------------------------------------------------------"), true);

  const bool ok = leaf.readData();
  printReadResult(ok);

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}