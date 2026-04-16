#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RS485AddressChangeExample.h"
#include "HondeMiniWeather5in1.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static HondeMiniWeather5in1 weather(
    rs485,
    SENSOR_ID,
    SENSOR_ADDRESS,
    SENSOR_DEBUG,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Honde MINI Weather 5-in-1 Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Optional scan in setup()"), true);
  printer.println(F("- Optional address change at boot"), true);
  printer.println(F("- Loop reads air temp, RH, pressure, wind speed, direction"), true);
  printer.println(F(""), true);
}

static void printReadResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(weather.getSensorId(), true, " | ");
  printer.print(F("Address: 0x"), true);
  printer.print((unsigned int)weather.getAddress(), true, " | ", HEX);
  printer.println("", true);

  if (ok) {
    printer.println(F("[APP] Successfully Read Values:"), true);
  } else {
    printer.println(F("[APP] Read failed. Current driver attributes:"), true);
  }

  printer.print(F("Air temp: "), true);
  printer.print(weather.air_temp, true, " C | ", 1);
  printer.print(F("RH: "), true);
  printer.print(weather.air_humidity, true, " % | ", 1);
  printer.print(F("Pressure: "), true);
  printer.print(weather.air_pressure, true, " hPa", 1);
  printer.println("", true);

  printer.print(F("Wind speed: "), true);
  printer.print(weather.wind_speed, true, " m/s | ", 2);
  printer.print(F("Wind direction: "), true);
  printer.print((unsigned int)weather.wind_direction, true, " deg");
  printer.println("", true);

  if (!ok) {
    printer.print(F("[APP] Error count: "), true);
    printer.println((unsigned int)weather.getConsecutiveErrors(), true);
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

  // Optional persistent address write. Controlled only by config.h macros.
  // If ADDRESS_CHANGE_BUTTON_PIN is defined, this waits up to 5 seconds for
  // the button before calling weather.changeAddress().
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(weather,
                           printer,
                           ADDRESS_CHANGE_NEW_ADDRESS,
                           F("Only the target sensor should be connected; Honde MINI writes address register 0x0020."));
  }

  if (DO_SCAN) {
    printer.println(F("[APP] Scan mode enabled. Searching sensor address..."), true);
    const uint8_t found = weather.scanForAddress(1, 247, 150, SENSOR_DEFAULT_AFTER_REQ_MS);
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

  const bool ok = weather.readData();
  printReadResult(ok);

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}
