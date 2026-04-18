#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXCT_WeatherStationSensors.h"

static JXCT_WindSpeed windSpeed(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    WIND_SPEED_MAX_M_S,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXCT Wind Speed Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Register 0x0016 = wind speed raw / 10 m/s"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(windSpeed.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)windSpeed.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("Wind speed raw: "), true);
  printer.print((unsigned int)windSpeed.wind_speed_raw, true, " | ", DEC);
  printer.print(F("Wind speed: "), true);
  printer.print(windSpeed.wind_speed_m_s, true, " m/s", 1);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(windSpeed, printer, ADDRESS_CHANGE_NEW_ADDRESS, F("Only the target wind speed sensor should be connected."));
  }
  if (DO_SCAN) {
    const uint8_t found = windSpeed.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printResult(windSpeed.readData());
  delay(POLL_INTERVAL_MS);
}
