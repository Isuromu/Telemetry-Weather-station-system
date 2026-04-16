#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RS485AddressChangeExample.h"
#include "RikaSoilSensor3in1.h"

// ESP32 boards can choose hardware UARTs explicitly. Other supported boards
// use Arduino's default Serial object for debug output.
#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

// PrintController wraps Serial-style output and keeps all example logging in
// one place. The second argument disables timestamps/prefixes here.
static PrintController printer(DebugPort, false);

// Shared Modbus RTU transport. The sensor driver below sends all requests
// through this bus object.
static RS485Bus rs485;

// Driver instance for one physical Rika 3-in-1 soil sensor.
// Most settings come from config.h or the global project configuration:
// - SENSOR_* identifies the Modbus node and enables optional debug traces.
// - POWERLINE/PORT indexes tell the larger system which hardware channel is used.
// - SAMPLE_RATE, warm-up, error, and power-off values feed SensorDriver behavior.
static RikaSoilSensor3in1 soil(
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

// Prints a short startup header so the serial monitor clearly shows which
// example is running and what optional diagnostics may be enabled.
static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Rika Soil Sensor 3-in-1 Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.println(F("- Optional scan in setup()"), true);
  printer.println(F("- Optional address change at boot"), true);
  printer.println(F("- Loop reads temp, VWC, EC"), true);
  printer.println(F("- Optional loop reads soil type and epsilon"), true);
  printer.println(F(""), true);
}

// Shows the result of the main measurement read. readData() updates the
// driver's public cached values, so this function prints those fields whether
// the latest read succeeded or failed.
static void printMainReadResult(bool ok) {
  printer.print(F("[APP] Sensor ID: "), true);
  printer.print(soil.getSensorId(), true, " | ");
  printer.print(F("Address: 0x"), true);
  printer.print((unsigned int)soil.getAddress(), true, " | ", HEX);
  printer.println("", true);

  if (ok) {
    printer.println(F("[APP] Successfully Read Values:"), true);
  } else {
    printer.println(F("[APP] Read failed. Current driver attributes:"), true);
  }

  // These values are decoded by RikaSoilSensor3in1::readData().
  // On failure the driver normally exposes fallback sentinel values.
  printer.print(F("Temp: "), true);
  printer.print(soil.soil_temp, true, " C° | ", 1);
  printer.print(F("VWC: "), true);
  printer.print(soil.soil_vwc, true, " % | ", 1);
  printer.print(F("EC: "), true);
  printer.print(soil.soil_ec, true, " mS/cm", 4);
  printer.println("", true);

  if (!ok) {
    printer.print(F("[APP] Error count: "), true);
    printer.println((unsigned int)soil.getConsecutiveErrors(), true);
  }
}

// Optional diagnostic: reads the configured soil class register and prints
// the enum as a human-readable label.
static void printSoilTypeResult(bool ok, RikaSoilSensor3in1::SoilType type) {
  if (ok) {
    printer.print(F("[APP] Soil type: "), true);
    printer.println(RikaSoilSensor3in1::soilTypeToString(type), true);
  } else {
    printer.println(F("[APP] Failed to read soil type"), true);
  }
}

// Optional diagnostic: epsilon is the dielectric constant reported by the
// sensor. It can help validate VWC behavior or calibration.
static void printEpsilonResult(bool ok, double eps) {
  if (ok) {
    printer.print(F("[APP] Epsilon: "), true);
    printer.println(eps, true, "", 2);
  } else {
    printer.println(F("[APP] Failed to read epsilon"), true);
  }
}

// Small helper for optional coefficient reads. The label is stored as a flash
// string with F("...") so RAM usage stays low on microcontrollers.
static void printCoeffResult(const __FlashStringHelper* label, bool ok, double value) {
  printer.print(label, true);
  if (ok) {
    printer.println(value, true, "", 2);
  } else {
    printer.println(F("FAILED"), true);
  }
}

void setup() {
  // Start the USB/debug serial port first so setup messages are visible.
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);
  printBanner();

  // Let the RS485 layer emit debug messages through the same printer used by
  // this example. SENSOR_DEBUG controls driver-level verbosity separately.
  rs485.setDebug(&printer);

  // Open the hardware UART used for RS485. ESP32 needs explicit RX/TX pins;
  // boards with fixed Serial2 pins can pass -1 for the pin arguments.
#if defined(ARDUINO_ARCH_ESP32)
  rs485.begin(RS485Port,
              RS485_DEFAULT_BAUD,
              PCB_RS485_RX_PINS[RS485_PORT_INDEX_0],
              PCB_RS485_TX_PINS[RS485_PORT_INDEX_0],
              RS485_DEFAULT_SERIAL_CONFIG);
#else
  rs485.begin(Serial2, RS485_DEFAULT_BAUD, -1, -1, RS485_DEFAULT_SERIAL_CONFIG);
#endif

  // RS485 is half-duplex: the DE pin switches the transceiver between
  // transmit and receive. The active level depends on the PCB/transceiver.
  rs485.setDirectionControl(PCB_RS485_DE_PINS[RS485_PORT_INDEX_0],
                            PCB_RS485_DE_ACTIVE_HIGH[RS485_PORT_INDEX_0]);

  // Optional persistent address write. Controlled only by config.h macros.
  // If ADDRESS_CHANGE_BUTTON_PIN is defined, this waits up to 5 seconds for
  // the button before calling soil.changeAddress().
  if (ADDRESS_CHANGE_AT_BOOT) {
    runAddressChangeAtBoot(soil,
                           printer,
                           ADDRESS_CHANGE_NEW_ADDRESS,
                           F("Only the target sensor should be connected; Rika uses address 0xFE and register 0x0200."));
  }

  // Optional address discovery. Enable DO_SCAN in config.h when you do not
  // know the sensor's Modbus address. A return value of 0 means no reply.
  if (DO_SCAN) {
    printer.println(F("[APP] Scan mode enabled. Searching sensor address..."), true);
    const uint8_t found = soil.scanForAddress(1, 247, 120, 20);
    if (found != 0) {
      printer.print(F("[APP] Sensor found at address 0x"), true);
      printer.println((unsigned int)found, true, "", HEX);
    } else {
      printer.println(F("[APP] No sensor responded during scan."), true);
    }
    printer.println(F(""), true);
  }

  // Optional one-time configuration write. This changes the soil type register
  // in the sensor, so keep SET_SOIL_TYPE_ON_BOOT false unless you intend that.
  if (SET_SOIL_TYPE_ON_BOOT) {
    printer.print(F("[APP] Setting soil type at boot to value "), true);
    printer.println((unsigned int)BOOT_SOIL_TYPE, true);

    // The final arguments are retry count, read timeout, and delay after each
    // request. They are kept explicit here so test tuning is easy.
    if (soil.setSoilType((RikaSoilSensor3in1::SoilType)BOOT_SOIL_TYPE, 3, 500, 20)) {
      printer.println(F("[APP] Soil type set successfully."), true);
    } else {
      printer.println(F("[APP] Failed to set soil type."), true);
    }
    printer.println(F(""), true);
  }
}

void loop() {
  // Each loop is one polling cycle. The separator makes repeated readings easy
  // to spot in a serial monitor log.
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] New polling cycle"), true);
  printer.println(F("------------------------------------------------------------"), true);

  // Main measurement read: temperature, volumetric water content, and EC are
  // requested in one Modbus transaction and cached on the soil object.
  const bool ok = soil.readData();
  printMainReadResult(ok);

  // Optional maintenance reads are disabled by default because each one adds
  // extra Modbus traffic and time to the polling cycle.
  if (READ_SOIL_TYPE_IN_LOOP) {
    RikaSoilSensor3in1::SoilType type = RikaSoilSensor3in1::SOIL_UNKNOWN;
    const bool typeOk = soil.readSoilType(type, 3, 500, 20);
    printSoilTypeResult(typeOk, type);
  }

  if (READ_EPSILON_IN_LOOP) {
    double eps = -99.0;
    const bool epsOk = soil.readEpsilon(eps, 3, 500, 20);
    printEpsilonResult(epsOk, eps);
  }

  if (READ_COMP_COEFFS_IN_LOOP) {
    double coeff = -99.0;

    // Compensation coefficients live in separate registers, so they are read
    // one at a time and printed with labels.
    bool okCoeff = soil.readCompensationCoeff(REG_EC_TEMP_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] EC temp coeff: "), okCoeff, coeff);

    okCoeff = soil.readCompensationCoeff(REG_SALINITY_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] Salinity coeff: "), okCoeff, coeff);

    okCoeff = soil.readCompensationCoeff(REG_TDS_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] TDS coeff: "), okCoeff, coeff);
  }

  printer.println(F(""), true);

  // Wait before the next cycle. Change POLL_INTERVAL_MS in config.h to adjust
  // how often the example talks to the sensor.
  delay(POLL_INTERVAL_MS);
}
