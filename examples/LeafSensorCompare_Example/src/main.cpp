#include <Arduino.h>
#include "config.h"
#include "PrintController.h"
#include "RS485Modbus.h"
#include "HondeLeafTemperatureHumidity.h"
#include "JXBS_LeafSurfaceHumidity.h"

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;
HardwareSerial RS485Port(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485;

static HondeLeafTemperatureHumidity hondeLeaf(
    rs485,
    HONDE_LEAF_SENSOR_ID,
    HONDE_LEAF_SENSOR_ADDRESS,
    HONDE_LEAF_SENSOR_DEBUG,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static JXBS_LeafSurfaceHumidity jxbsLeaf(
    rs485,
    JXBS_LEAF_SENSOR_ID,
    JXBS_LEAF_SENSOR_ADDRESS,
    JXBS_LEAF_SENSOR_DEBUG,
    POWERLINE_INDEX_0,
    RS485_PORT_INDEX_0,
    SAMPLE_RATE_15_MIN,
    1000UL,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Honde vs JXBS Leaf Sensor Compare Example"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.println(F("- Reads Honde leaf sensor first"), true);
  printer.println(F("- Then reads JXBS leaf sensor"), true);
  printer.println(F("- Driver debug is enabled for both sensors"), true);
  printer.println(F("- Summary prints both values and deltas"), true);
  printer.println(F(""), true);
}

static void beginRS485() {
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
}

static void printSensorHeader(const __FlashStringHelper* label,
                              const char* id,
                              uint8_t address) {
  printer.println(F("------------------------------------------------------------"), true);
  printer.print(F("[APP] Reading "), true);
  printer.print(label, true);
  printer.print(F(" | ID: "), true);
  printer.print(id, true);
  printer.print(F(" | Address: 0x"), true);
  printer.println((unsigned int)address, true, "", HEX);
}

static void printIndividualResult(const __FlashStringHelper* label,
                                  bool ok,
                                  double temperature,
                                  double humidity,
                                  uint8_t errors) {
  printer.print(F("[APP] "), true);
  printer.print(label, true);
  printer.print(F(" result: "), true);

  if (ok) {
    printer.print(F("OK | temperature="), true);
    printer.print(temperature, true, " C | ", 2);
    printer.print(F("humidity="), true);
    printer.print(humidity, true, " %RH", 2);
    printer.println("", true);
  } else {
    printer.print(F("FAIL | cached temperature="), true);
    printer.print(temperature, true, " C | ", 2);
    printer.print(F("cached humidity="), true);
    printer.print(humidity, true, " %RH | errors=", 2);
    printer.println((unsigned int)errors, true);
  }
}

static void printComparisonSummary(bool hondeOk, bool jxbsOk) {
  printer.println(F("------------------------------------------------------------"), true);
  printer.println(F("[APP] Comparison summary"), true);

  printIndividualResult(F("Honde"),
                        hondeOk,
                        hondeLeaf.leaf_temperature,
                        hondeLeaf.leaf_humidity,
                        hondeLeaf.getConsecutiveErrors());

  printIndividualResult(F("JXBS"),
                        jxbsOk,
                        jxbsLeaf.leaf_temperature,
                        jxbsLeaf.leaf_humidity,
                        jxbsLeaf.getConsecutiveErrors());

  if (hondeOk && jxbsOk) {
    const double tempDelta = jxbsLeaf.leaf_temperature - hondeLeaf.leaf_temperature;
    const double humidityDelta = jxbsLeaf.leaf_humidity - hondeLeaf.leaf_humidity;

    printer.print(F("[APP] Delta JXBS-Honde: temperature="), true);
    printer.print(tempDelta, true, " C | ", 2);
    printer.print(F("humidity="), true);
    printer.print(humidityDelta, true, " %RH", 2);
    printer.println("", true);
  } else {
    printer.println(F("[APP] Delta skipped because at least one sensor read failed."), true);
  }

  printer.println(F(""), true);
}

void setup() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);

  printBanner();
  beginRS485();
}

void loop() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F("[APP] New comparison cycle"), true);

  printSensorHeader(F("Honde leaf sensor"),
                    hondeLeaf.getSensorId(),
                    hondeLeaf.getAddress());
  const bool hondeOk = hondeLeaf.readData();

  delay(BETWEEN_SENSOR_READ_DELAY_MS);

  printSensorHeader(F("JXBS leaf sensor"),
                    jxbsLeaf.getSensorId(),
                    jxbsLeaf.getAddress());
  const bool jxbsOk = jxbsLeaf.readData();

  printComparisonSummary(hondeOk, jxbsOk);

  delay(POLL_INTERVAL_MS);
}
