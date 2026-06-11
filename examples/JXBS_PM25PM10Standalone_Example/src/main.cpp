#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXBS_PM25PM10Standalone.h"

static JXBS_PM25PM10Standalone pmSensor(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    PM_MAX_UG_M3,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS Standalone PM2.5/PM10 Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Register 0x0004 = PM2.5 raw ug/m3"), true);
  printer.println(F("- Register 0x0009 = PM10 raw ug/m3"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(pmSensor.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)pmSensor.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("PM2.5 raw: "), true);
  printer.print((unsigned int)pmSensor.pm2_5_raw, true, " | ", DEC);
  printer.print(F("PM2.5: "), true);
  printer.print(pmSensor.pm2_5_ug_m3, true, " ug/m3 | ", 0);
  printer.print(F("PM10 raw: "), true);
  printer.print((unsigned int)pmSensor.pm10_raw, true, " | ", DEC);
  printer.print(F("PM10: "), true);
  printer.print(pmSensor.pm10_ug_m3, true, " ug/m3", 0);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();

  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(pmSensor,
                           printer,
                           ADDRESS_CHANGE_NEW_ADDRESS,
                           F("Only the target standalone PM sensor should be connected."));
  }

  if (DO_SCAN) {
    const uint8_t found = pmSensor.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printResult(pmSensor.readData());
  delay(POLL_INTERVAL_MS);
}
