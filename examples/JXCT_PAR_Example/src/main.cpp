#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXCT_WeatherStationSensors.h"

static JXCT_PAR parSensor(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    PAR_SCALE_DIVISOR,
    PAR_MAX_VALUE,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXCT PAR Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Register 0x0006 = PAR value"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(parSensor.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)parSensor.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("PAR raw: "), true);
  printer.print((unsigned int)parSensor.par_raw, true, " | ", DEC);
  printer.print(F("PAR value: "), true);
  printer.print(parSensor.par_value, true, " sensor-unit", 2);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(parSensor, printer, ADDRESS_CHANGE_NEW_ADDRESS, F("Only the target PAR sensor should be connected."));
  }
  if (DO_SCAN) {
    const uint8_t found = parSensor.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printResult(parSensor.readData());
  delay(POLL_INTERVAL_MS);
}
