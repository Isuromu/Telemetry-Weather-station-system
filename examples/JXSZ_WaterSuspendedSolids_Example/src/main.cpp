#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RS485AddressChangeExample.h"
#include "JXSZ_WaterSuspendedSolids.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static JXSZ_WaterSuspendedSolids suspendedSolids(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    SUSPENDED_SOLIDS_SCALE_DIVISOR,
    SUSPENDED_SOLIDS_MAX_MG_L,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXSZ Water Suspended Solids Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.println(F("- Combined read: temperature + suspended solids"), true);
  printer.println(F("- Default scaling: suspended solids raw / 10"), true);
  printer.println(F("- Change SUSPENDED_SOLIDS_SCALE_DIVISOR if your unit uses /100"), true);
  printer.println(F(""), true);
}

static void printMainReadResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(suspendedSolids.getSensorId(), true, " | ");
  printer.print(F("Address: 0x"), true);
  printer.print((unsigned int)suspendedSolids.getAddress(), true, " | ", HEX);
  printer.println("", true);

  if (ok) {
    printer.println(F("[APP] Successfully Read Values:"), true);
  } else {
    printer.println(F("[APP] Read failed. Current driver attributes:"), true);
  }

  printer.print(F("Temp: "), true);
  printer.print(suspendedSolids.water_temperature_C, true, " C | ", 1);
  printer.print(F("SS raw: "), true);
  printer.print((unsigned int)suspendedSolids.suspended_solids_raw, true, " | ", DEC);
  printer.print(F("SS: "), true);
  printer.print(suspendedSolids.suspended_solids_mg_L, true, " mg/L", 2);
  printer.println("", true);

  if (!ok) {
    printer.print(F("[APP] Error count: "), true);
    printer.println((unsigned int)suspendedSolids.getConsecutiveErrors(), true);
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

  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(suspendedSolids,
                           printer,
                           ADDRESS_CHANGE_NEW_ADDRESS,
                           F("Only the target JXSZ suspended solids sensor should be connected; address register is 0x0100."));
  }

  if (DO_SCAN) {
    printer.println(F("[APP] Scan mode enabled. Searching JXSZ suspended solids address..."), true);
    const uint8_t found = suspendedSolids.scanForAddress(1, 247, 150, 20);
    if (found != 0) {
      printer.print(F("[APP] Sensor found at address 0x"), true);
      printer.println((unsigned int)found, true, "", HEX);
    } else {
      printer.println(F("[APP] No sensor responded during scan."), true);
    }
    printer.println(F(""), true);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] New polling cycle"), true);
  printer.println(F("------------------------------------------------------------"), true);

  const bool ok = suspendedSolids.readData();
  printMainReadResult(ok);

  if (READ_TEMPERATURE_ONLY_IN_LOOP) {
    const bool tempOk = suspendedSolids.readTemperature(3, 500, 20);
    printer.print(F("[APP] Temperature-only read: "), true);
    printer.println(tempOk ? F("OK") : F("FAILED"), true);
  }

  if (READ_SUSPENDED_SOLIDS_ONLY_IN_LOOP) {
    const bool ssOk = suspendedSolids.readSuspendedSolids(3, 500, 20);
    printer.print(F("[APP] Suspended-solids-only read: "), true);
    printer.println(ssOk ? F("OK") : F("FAILED"), true);
  }

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}
