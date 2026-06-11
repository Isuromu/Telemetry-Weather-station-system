#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXCT_AirQualityShield.h"

static JXCT_AirQualityShield airQuality(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    AIR_PM_MAX_UG_M3,
    AIR_TVOC_MAX_PPB,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXCT Air Quality Shield Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.println(F("- Reads humidity, temperature, PM2.5, TVOC, PM10"), true);
  printer.println(F("- Register map: H=0x0000 T=0x0001 PM2.5=0x0004 TVOC=0x0006 PM10=0x0009"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(airQuality.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)airQuality.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("Humidity: "), true);
  printer.print(airQuality.humidity_percent, true, " %RH | ", 1);
  printer.print(F("Temp: "), true);
  printer.print(airQuality.air_temperature_C, true, " C | ", 1);
  printer.print(F("PM2.5: "), true);
  printer.print(airQuality.pm2_5_ug_m3, true, " ug/m3 | ", 0);
  printer.print(F("TVOC: "), true);
  printer.print(airQuality.tvoc_ppb, true, " ppb | ", 0);
  printer.print(F("PM10: "), true);
  printer.print(airQuality.pm10_ug_m3, true, " ug/m3", 0);
  printer.println("", true);
  if (!ok) {
    printer.print(F("[APP] Error count: "), true);
    printer.println((unsigned int)airQuality.getConsecutiveErrors(), true);
  }
}

void setup() {
  beginRS485SensorExample();
  printBanner();

  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(airQuality,
                           printer,
                           ADDRESS_CHANGE_NEW_ADDRESS,
                           F("Only the target air quality shield should be connected; address register is 0x0100."));
  }

  if (DO_SCAN) {
    const uint8_t found = airQuality.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printResult(airQuality.readData());
  delay(POLL_INTERVAL_MS);
}
