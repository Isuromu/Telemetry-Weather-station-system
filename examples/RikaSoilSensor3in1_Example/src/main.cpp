#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaSoilSensor3in1.h"

/*
  Rika Soil Sensor 3-in-1 Example

  Runtime flow:
  1) Initialize debug serial and RS485 bus.
  2) Optionally scan Modbus addresses.
  3) Optionally set soil type during boot.
  4) Poll primary values (temp, VWC, EC).
  5) Optionally poll soil type, epsilon, and compensation coefficients.

  Keep this example focused on diagnostics and protocol visibility.
*/
#if defined(ARDUINO_ARCH_ESP32)
// ESP32: debug on UART0, RS485 on UART1.
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
// Non-ESP32 fallback: debug on default Serial.
#define DebugPort Serial
#endif

// Logging facade and RS485 transport used by the soil sensor driver.
static PrintController printer(DebugPort, false);
static RS485Bus rs485;

// Soil driver instance with application policy values from config + global configs.
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

// Prints a startup summary in serial monitor.
static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Rika Soil Sensor 3-in-1 Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Optional scan in setup()"), true);
  printer.println(F("- Loop reads temp, VWC, EC"), true);
  printer.println(F("- Optional loop reads soil type and epsilon"), true);
  printer.println(F(""), true);
}

// Prints result of the main readData() transaction.
static void printMainReadResult(bool ok) {
  if (ok) {
    printer.print(F("[APP] Sensor ID: "), true);
    printer.print(soil.getSensorId(), true, " | ");

    printer.print(F("Address: 0x"), true);
    printer.print((unsigned int)soil.getAddress(), true, " | ", HEX);

    printer.println("", true);
    printer.print(F("[APP] Successfully Read Values: "), true, "| ");

    printer.print(F("Temp: "), true);
    printer.print(soil.soil_temp, true, " C | ", 1);

    printer.print(F("VWC: "), true);
    printer.print(soil.soil_vwc, true, " % | ", 1);

    printer.print(F("EC: "), true);
    printer.print(soil.soil_ec, true, " mS/cm", 4);
    printer.println("", true);
  } else {
    printer.print(F("[APP] Read failed for sensor "), true);
    printer.print(soil.getSensorId(), true, " | ");
    printer.print(F("Error count: "), true);
    printer.println((unsigned int)soil.getConsecutiveErrors(), true);
  }
}

// Prints normalized soil type name returned by readSoilType().
static void printSoilTypeResult(bool ok, RikaSoilSensor3in1::SoilType type) {
  if (ok) {
    printer.print(F("[APP] Soil type: "), true);
    printer.println(RikaSoilSensor3in1::soilTypeToString(type), true);
  } else {
    printer.println(F("[APP] Failed to read soil type"), true);
  }
}

// Prints dielectric constant (epsilon) read status/value.
static void printEpsilonResult(bool ok, double eps) {
  if (ok) {
    printer.print(F("[APP] Epsilon: "), true);
    printer.println(eps, true, "", 2);
  } else {
    printer.println(F("[APP] Failed to read epsilon"), true);
  }
}

// Generic helper for optional compensation-coefficient reads.
static void printCoeffResult(const __FlashStringHelper* label, bool ok, double value) {
  printer.print(label, true);
  if (ok) {
    printer.println(value, true, "", 2);
  } else {
    printer.println(F("FAILED"), true);
  }
}

void setup() {
  // 1) Bring up debug serial first so all setup steps are visible.
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);
  printBanner();

  // 2) Hook RS485 debug logging to the shared printer.
  rs485.setDebug(&printer);

#if defined(ARDUINO_ARCH_ESP32)
  // 3) Initialize RS485 UART and board-specific RX/TX pins.
  rs485.begin(RS485Port,
              RS485_DEFAULT_BAUD,
              PCB_RS485_RX_PINS[RS485_PORT_INDEX_0],
              PCB_RS485_TX_PINS[RS485_PORT_INDEX_0],
              RS485_DEFAULT_SERIAL_CONFIG);
#else
  // Non-ESP32 fallback signature.
  rs485.begin(Serial2, RS485_DEFAULT_BAUD, -1, -1, RS485_DEFAULT_SERIAL_CONFIG);
#endif

  // 4) Configure DE/RE control for half-duplex RS485 transceiver.
  rs485.setDirectionControl(PCB_RS485_DE_PINS[RS485_PORT_INDEX_0],
                            PCB_RS485_DE_ACTIVE_HIGH[RS485_PORT_INDEX_0]);

  // 5) Optional one-time address discovery sweep.
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

  // 6) Optional boot-time soil type write (persisted by sensor).
  if (SET_SOIL_TYPE_ON_BOOT) {
    printer.print(F("[APP] Setting soil type at boot to value "), true);
    printer.println((unsigned int)BOOT_SOIL_TYPE, true);

    if (soil.setSoilType((RikaSoilSensor3in1::SoilType)BOOT_SOIL_TYPE, 3, 500, 20)) {
      printer.println(F("[APP] Soil type set successfully."), true);
    } else {
      printer.println(F("[APP] Failed to set soil type."), true);
    }
    printer.println(F(""), true);
  }
}

void loop() {
  // Periodic diagnostic polling cycle.
  printer.println(F(""), true);
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] New polling cycle"), true);
  printer.println(F("------------------------------------------------------------"), true);

  const bool ok = soil.readData();
  printMainReadResult(ok);

  // Optional extended diagnostic calls.
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

    // These registers are configured in config.h.
    bool okCoeff = soil.readCompensationCoeff(REG_EC_TEMP_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] EC temp coeff: "), okCoeff, coeff);

    okCoeff = soil.readCompensationCoeff(REG_SALINITY_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] Salinity coeff: "), okCoeff, coeff);

    okCoeff = soil.readCompensationCoeff(REG_TDS_COEFF, coeff, 3, 500, 20);
    printCoeffResult(F("[APP] TDS coeff: "), okCoeff, coeff);
  }

  printer.println(F(""), true);
  delay(POLL_INTERVAL_MS);
}
