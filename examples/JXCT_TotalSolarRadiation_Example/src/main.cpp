#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXCT_TotalSolarRadiation.h"

static JXCT_TotalSolarRadiation solarSensor(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    SOLAR_MAX_W_M2,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXCT Total Solar Radiation Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Register 0x0000 = total solar radiation W/m2"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(solarSensor.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)solarSensor.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("Solar raw: "), true);
  printer.print((unsigned int)solarSensor.solar_raw, true, " | ", DEC);
  printer.print(F("Solar: "), true);
  printer.print(solarSensor.total_solar_w_m2, true, " W/m2", 0);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(solarSensor, printer, ADDRESS_CHANGE_NEW_ADDRESS, F("Only the target solar radiation sensor should be connected."));
  }
  if (DO_SCAN) {
    const uint8_t found = solarSensor.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printResult(solarSensor.readData());
  delay(POLL_INTERVAL_MS);
}
