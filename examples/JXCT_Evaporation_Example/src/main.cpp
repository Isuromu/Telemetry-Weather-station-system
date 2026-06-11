#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXCT_Evaporation.h"

static JXCT_Evaporation evaporationSensor(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    EVAPORATION_SCALE_DIVISOR,
    EVAPORATION_MAX_VALUE,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXCT Evaporation Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Register 0x0006 = evaporation value"), true);
  printer.println(F("- Optional tare command: write 0x0001 to register 0x0102"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(evaporationSensor.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)evaporationSensor.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("Evaporation raw: "), true);
  printer.print((unsigned int)evaporationSensor.evaporation_raw, true, " | ", DEC);
  printer.print(F("Evaporation value: "), true);
  printer.print(evaporationSensor.evaporation_value, true, " sensor-unit", 2);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(evaporationSensor, printer, ADDRESS_CHANGE_NEW_ADDRESS, F("Only the target evaporation sensor should be connected."));
  }
  if (TARE_AT_BOOT) {
    printer.println(F("[APP] Sending evaporation tare command."), true);
    printer.println(evaporationSensor.tare() ? F("[APP] Tare OK") : F("[APP] Tare FAILED"), true);
  }
  if (DO_SCAN) {
    const uint8_t found = evaporationSensor.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printResult(evaporationSensor.readData());
  delay(POLL_INTERVAL_MS);
}
