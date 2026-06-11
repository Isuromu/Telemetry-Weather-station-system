#include <Arduino.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"
#include "RS485AddressChangeExample.h"
#include "JXCT_UVRays.h"

static JXCT_UVRays uvSensor(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    UV_MAX_W_M2,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXCT UV Rays Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Registers: humidity 0x0000, temperature 0x0001, UV 0x0008"), true);
  printer.println(F(""), true);
}

static void printResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(uvSensor.getSensorId(), true, " | Address: 0x");
  printer.println((unsigned int)uvSensor.getAddress(), true, "", HEX);
  printer.println(ok ? F("[APP] Successfully Read Values:") : F("[APP] Read failed. Current driver attributes:"), true);
  printer.print(F("Humidity: "), true);
  printer.print(uvSensor.humidity_percent, true, " %RH | ", 1);
  printer.print(F("Temp: "), true);
  printer.print(uvSensor.air_temperature_C, true, " C | ", 1);
  printer.print(F("UV raw: "), true);
  printer.print((unsigned int)uvSensor.uv_raw, true, " | ", DEC);
  printer.print(F("UV: "), true);
  printer.print(uvSensor.uv_w_m2, true, " W/m2", 1);
  printer.println("", true);
}

void setup() {
  beginRS485SensorExample();
  printBanner();
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(uvSensor, printer, ADDRESS_CHANGE_NEW_ADDRESS, F("Only the target UV sensor should be connected."));
  }
  if (DO_SCAN) {
    const uint8_t found = uvSensor.scanForAddress(1, 247, 150, 20);
    printer.print(F("[APP] Scan result: 0x"), true);
    printer.println((unsigned int)found, true, "", HEX);
  }
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printResult(uvSensor.readData());
  delay(POLL_INTERVAL_MS);
}
