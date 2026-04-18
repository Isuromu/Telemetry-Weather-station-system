#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RS485AddressChangeExample.h"
#include "JXBS_LiquidPH.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static JXBS_LiquidPH phSensor(
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
  printer.println(F(" JXBS-3001-PH-RS Liquid pH Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.println(F("- Optional scan in setup()"), true);
  printer.println(F("- Optional address change at boot"), true);
  printer.println(F("- Main loop reads temperature + pH together"), true);
  printer.println(F("- Optional loop reads temperature-only and pH-only"), true);
  printer.println(F(""), true);

  printer.println(F("[INSTALL] Power supply: 12-24 VDC"), true);
  printer.println(F("[INSTALL] Manual wiring: Brown=V+, Black=V-, Yellow/Grey=RS485-A, Blue=RS485-B"), true);
  printer.println(F("[INSTALL] If your cable is Red/Black/Yellow/Green: likely Red=V+, Black=V-, Yellow=A, Green=B; verify before powering."), true);
  printer.println(F("[INSTALL] Keep the pH electrode wet or in its storage/protective cap when not installed."), true);
  printer.println(F(""), true);
}

static void printReadResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(phSensor.getSensorId(), true, " | ");
  printer.print(F("Address: 0x"), true);
  printer.print((unsigned int)phSensor.getAddress(), true, " | ", HEX);
  printer.println("", true);

  if (ok) {
    printer.println(F("[APP] Successfully Read Values:"), true);
  } else {
    printer.println(F("[APP] Read failed. Current driver attributes:"), true);
  }

  printer.print(F("Liquid temperature: "), true);
  printer.print(phSensor.liquid_temperature, true, " C | ", 1);
  printer.print(F("pH: "), true);
  printer.print(phSensor.liquid_ph, true, "", 2);
  printer.println("", true);

  if (!ok) {
    printer.print(F("[APP] Error count: "), true);
    printer.println((unsigned int)phSensor.getConsecutiveErrors(), true);
  }
}

static void printTemperatureOnlyResult(bool ok) {
  if (ok) {
    printer.print(F("[APP] Temperature-only read: "), true);
    printer.print(phSensor.liquid_temperature, true, " C", 1);
    printer.println("", true);
  } else {
    printer.println(F("[APP] Temperature-only read failed."), true);
  }
}

static void printPHOnlyResult(bool ok) {
  if (ok) {
    printer.print(F("[APP] pH-only read: "), true);
    printer.println(phSensor.liquid_ph, true, "", 2);
  } else {
    printer.println(F("[APP] pH-only read failed."), true);
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
  // the button before calling phSensor.changeAddress().
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(phSensor,
                           printer,
                           ADDRESS_CHANGE_NEW_ADDRESS,
                           F("Only the target sensor should be connected; JXBS pH writes address register 0x0100."));
  }

  if (DO_SCAN) {
    printer.println(F("[APP] Scan mode enabled. Searching sensor address..."), true);
    const uint8_t found = phSensor.scanForAddress(1, 247, 150, SENSOR_DEFAULT_AFTER_REQ_MS);
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

  const bool ok = phSensor.readData();
  printReadResult(ok);

  if (READ_TEMPERATURE_ONLY_IN_LOOP) {
    const bool tempOk = phSensor.readTemperature(3, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS);
    printTemperatureOnlyResult(tempOk);
  }

  if (READ_PH_ONLY_IN_LOOP) {
    const bool phOk = phSensor.readPH(3, SENSOR_DEFAULT_READ_TIMEOUT_MS, SENSOR_DEFAULT_AFTER_REQ_MS);
    printPHOnlyResult(phOk);
  }

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}
