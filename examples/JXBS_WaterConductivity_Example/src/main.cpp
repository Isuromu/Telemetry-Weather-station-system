#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RS485AddressChangeExample.h"
#include "JXBS_WaterConductivity.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static JXBS_WaterConductivity waterConductivity(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    CONDUCTIVITY_SCALE_DIVISOR,
    CONDUCTIVITY_MAX_US_CM,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS Water Conductivity Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.println(F("- Requires RS485 transmitter/controller, not bare probe only"), true);
  printer.println(F("- Combined read: temperature + conductivity"), true);
  printer.println(F("- Default K=1 scaling: conductivity raw / 100"), true);
  printer.println(F(""), true);
}

static void printMainReadResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(waterConductivity.getSensorId(), true, " | ");
  printer.print(F("Address: 0x"), true);
  printer.print((unsigned int)waterConductivity.getAddress(), true, " | ", HEX);
  printer.println("", true);

  if (ok) {
    printer.println(F("[APP] Successfully Read Values:"), true);
  } else {
    printer.println(F("[APP] Read failed. Current driver attributes:"), true);
  }

  printer.print(F("Temp: "), true);
  printer.print(waterConductivity.water_temperature_C, true, " C | ", 1);
  printer.print(F("EC raw: "), true);
  printer.print((unsigned long)waterConductivity.conductivity_raw, true, " | ", DEC);
  printer.print(F("EC: "), true);
  printer.print(waterConductivity.conductivity_uS_cm, true, " uS/cm", 2);
  printer.println("", true);

  if (!ok) {
    printer.print(F("[APP] Error count: "), true);
    printer.println((unsigned int)waterConductivity.getConsecutiveErrors(), true);
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
    runAddressChangeAtBoot(waterConductivity,
                           printer,
                           ADDRESS_CHANGE_NEW_ADDRESS,
                           F("Only the target water conductivity controller should be connected; address register is 0x0100."));
  }

  if (DO_SCAN) {
    printer.println(F("[APP] Scan mode enabled. Searching water conductivity controller address..."), true);
    const uint8_t found = waterConductivity.scanForAddress(1, 247, 150, 20);
    if (found != 0) {
      printer.print(F("[APP] Controller found at address 0x"), true);
      printer.println((unsigned int)found, true, "", HEX);
    } else {
      printer.println(F("[APP] No controller responded during scan."), true);
    }
    printer.println(F(""), true);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] New polling cycle"), true);
  printer.println(F("------------------------------------------------------------"), true);

  const bool ok = waterConductivity.readData();
  printMainReadResult(ok);

  if (READ_TEMPERATURE_ONLY_IN_LOOP) {
    const bool tempOk = waterConductivity.readTemperature(3, 500, 20);
    printer.print(F("[APP] Temperature-only read: "), true);
    printer.println(tempOk ? F("OK") : F("FAILED"), true);
  }

  if (READ_CONDUCTIVITY_ONLY_IN_LOOP) {
    const bool ecOk = waterConductivity.readConductivity(3, 500, 20);
    printer.print(F("[APP] Conductivity-only read: "), true);
    printer.println(ecOk ? F("OK") : F("FAILED"), true);
  }

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}
