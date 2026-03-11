#include <Arduino.h>
#include "../config/Configuration.h"
#include "../config/Configuration_System.h"
#include "../config/Configuration_PCB.h"
#include "../config/Configuration_Sensors.h"

#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaLeafSensor.h"
#include "RikaSoilSensor3in1.h"

// ============================================================
// Debug port
// ============================================================
#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial& DebugPort = Serial0;   // same UART side as upload bridge
HardwareSerial RS485Hw0(1);
#else
#define DebugPort Serial
#endif

static PrintController printer(DebugPort, false);
static RS485Bus rs485Bus0;

// ============================================================
// Sensor object creation from station configuration
// ============================================================
#ifdef RIKA_LEAF_00_ENABLED
static RikaLeafSensor sensor_leaf_00(
    rs485Bus0,
    RIKA_LEAF_00_ID,
    RIKA_LEAF_00_ADDRESS,
    RIKA_LEAF_00_DEBUG,
    RIKA_LEAF_00_POWERLINE,
    RIKA_LEAF_00_RS485_PORT,
    RIKA_LEAF_00_SAMPLE_RATE,
    RIKA_LEAF_00_WARMUP_MS,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);
#endif

#ifdef RIKA_SOIL3IN1_00_ENABLED
static RikaSoilSensor3in1 sensor_soil_00(
    rs485Bus0,
    RIKA_SOIL3IN1_00_ID,
    RIKA_SOIL3IN1_00_ADDRESS,
    RIKA_SOIL3IN1_00_DEBUG,
    RIKA_SOIL3IN1_00_POWERLINE,
    RIKA_SOIL3IN1_00_RS485_PORT,
    RIKA_SOIL3IN1_00_SAMPLE_RATE,
    RIKA_SOIL3IN1_00_WARMUP_MS,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS);
#endif

static SensorDriver* g_sensors[] = {
#ifdef RIKA_LEAF_00_ENABLED
  &sensor_leaf_00,
#endif
#ifdef RIKA_SOIL3IN1_00_ENABLED
  &sensor_soil_00,
#endif
};

static const size_t g_sensorCount = sizeof(g_sensors) / sizeof(g_sensors[0]);

// ============================================================
// Runtime power/interface state tracking
// ============================================================
static bool g_powerLineState[PCB_POWERLINE_COUNT] = {false};
static bool g_rs485InterfaceState[PCB_RS485_PORT_COUNT] = {false};

// ============================================================
// Read plan structure
// ============================================================
struct ReadPlanEntry {
  SensorDriver* sensor;
};

static ReadPlanEntry g_readPlan[16];
static size_t g_readPlanCount = 0;

// ============================================================
// Helpers
// ============================================================
static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Primary Station Main - Sensor Architecture Test"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("Station ID: "), true);
  printer.println(STATION_ID, true);
  printer.print(F("PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.println(F(""), true);
}

static void initPowerLines() {
  for (uint8_t i = 0; i < PCB_POWERLINE_COUNT; ++i) {
    const int8_t pin = PCB_POWERLINE_SWITCH_PINS[i];
    if (pin >= 0) {
      pinMode(pin, OUTPUT);
      const bool activeHigh = PCB_POWERLINE_ACTIVE_HIGH[i];
      digitalWrite(pin, activeHigh ? LOW : HIGH); // OFF state
    }
    g_powerLineState[i] = false;
  }
}

static void initInterfaces() {
  for (uint8_t i = 0; i < PCB_RS485_PORT_COUNT; ++i) {
    const int8_t enPin = PCB_RS485_ENABLE_PINS[i];
    if (enPin >= 0) {
      pinMode(enPin, OUTPUT);
      const bool activeHigh = PCB_RS485_ENABLE_ACTIVE_HIGH[i];
      digitalWrite(enPin, activeHigh ? LOW : HIGH); // disabled
    }
    g_rs485InterfaceState[i] = false;
  }
}

static void powerLineSet(uint8_t index, bool on) {
  if (index >= PCB_POWERLINE_COUNT) return;

  const int8_t pin = PCB_POWERLINE_SWITCH_PINS[index];
  if (pin >= 0) {
    const bool activeHigh = PCB_POWERLINE_ACTIVE_HIGH[index];
    digitalWrite(pin, on ? (activeHigh ? HIGH : LOW)
                         : (activeHigh ? LOW  : HIGH));
  }
  g_powerLineState[index] = on;
}

static bool powerLineReadState(uint8_t index) {
  if (index >= PCB_POWERLINE_COUNT) return false;

  const int8_t statusPin = PCB_POWERLINE_STATUS_PINS[index];
  if (statusPin >= 0) {
    const bool activeHigh = PCB_POWERLINE_STATUS_ACTIVE_HIGH[index];
    const int raw = digitalRead(statusPin);
    return activeHigh ? (raw == HIGH) : (raw == LOW);
  }

  if (PCB_POWERLINE_SWITCH_PINS[index] < 0) {
    return true; // physically always on in current test hardware
  }

  return g_powerLineState[index];
}

static void rs485InterfaceSet(uint8_t index, bool on) {
  if (index >= PCB_RS485_PORT_COUNT) return;

  const int8_t pin = PCB_RS485_ENABLE_PINS[index];
  if (pin >= 0) {
    const bool activeHigh = PCB_RS485_ENABLE_ACTIVE_HIGH[index];
    digitalWrite(pin, on ? (activeHigh ? HIGH : LOW)
                         : (activeHigh ? LOW  : HIGH));
  }
  g_rs485InterfaceState[index] = on;
}

static bool hasPlannedSensorInGroup(uint8_t powerLine, uint8_t interfaceIndex) {
  for (size_t i = 0; i < g_readPlanCount; ++i) {
    if (g_readPlan[i].sensor->getPowerLineIndex() == powerLine &&
        g_readPlan[i].sensor->getInterfaceIndex() == interfaceIndex) {
      return true;
    }
  }
  return false;
}

static bool hasPlannedSensorOnPowerLine(uint8_t powerLine) {
  for (size_t i = 0; i < g_readPlanCount; ++i) {
    if (g_readPlan[i].sensor->getPowerLineIndex() == powerLine) {
      return true;
    }
  }
  return false;
}

static uint32_t getMaxWarmupForPowerLine(uint8_t powerLine) {
  uint32_t maxWarmup = 0;

  for (size_t i = 0; i < g_readPlanCount; ++i) {
    SensorDriver* sensor = g_readPlan[i].sensor;
    if (sensor->getPowerLineIndex() == powerLine) {
      if (sensor->getWarmUpTimeMs() > maxWarmup) {
        maxWarmup = sensor->getWarmUpTimeMs();
      }
    }
  }

  return maxWarmup;
}

static bool powerLineShouldStayOn(uint8_t powerLine) {
  if (powerLine >= PCB_POWERLINE_COUNT) return false;
  if (!PCB_POWERLINE_CAN_STAY_ON_IN_SLEEP[powerLine]) return false;

  for (size_t i = 0; i < g_sensorCount; ++i) {
    if (g_sensors[i]->getPowerLineIndex() == powerLine &&
        g_sensors[i]->shouldKeepPowerOn()) {
      return true;
    }
  }

  return false;
}

static void printSensorMap() {
  printer.println(F("Sensor map:"), true);

  for (size_t i = 0; i < g_sensorCount; ++i) {
    SensorDriver* s = g_sensors[i];
    printer.print(F("  ID="), true);
    printer.print(s->getSensorId(), true, " | ");
    printer.print(F("Addr=0x"), true);
    printer.print((unsigned int)s->getAddress(), true, " | ", HEX);
    printer.print(F("PowerLine="), true);
    printer.print((unsigned int)s->getPowerLineIndex(), true, " | ");
    printer.print(F("Interface="), true);
    printer.print((unsigned int)s->getInterfaceIndex(), true, " | ");
    printer.print(F("SampleMin="), true);
    printer.print((unsigned int)s->getSampleRateMin(), true, " | ");
    printer.print(F("WarmUpMs="), true);
    printer.print((unsigned long)s->getWarmUpTimeMs(), true, " | ");
    printer.print(F("KeepOn="), true);
    printer.println(s->shouldKeepPowerOn() ? F("true") : F("false"), true);
  }

  printer.println(F(""), true);
}

static void printDuePlan() {
  printer.println(F("[PLAN] Current due sensor plan:"), true);

  if (g_readPlanCount == 0) {
    printer.println(F("  <empty>"), true);
    printer.println(F(""), true);
    return;
  }

  for (size_t i = 0; i < g_readPlanCount; ++i) {
    SensorDriver* s = g_readPlan[i].sensor;
    printer.print(F("  "), true);
    printer.print(s->getSensorId(), true, " | ");
    printer.print(F("Pwr="), true);
    printer.print((unsigned int)s->getPowerLineIndex(), true, " | ");
    printer.print(F("If="), true);
    printer.print((unsigned int)s->getInterfaceIndex(), true, " | ");
    printer.print(F("SampleMin="), true);
    printer.println((unsigned int)s->getSampleRateMin(), true);
  }

  printer.println(F(""), true);
}

static size_t buildReadPlan(uint32_t nowMs) {
  g_readPlanCount = 0;

  for (size_t i = 0; i < g_sensorCount; ++i) {
    SensorDriver* s = g_sensors[i];

    if (!s->isOnline()) {
      continue;
    }

    if (!s->isDueForRead(nowMs)) {
      continue;
    }

    if (g_readPlanCount < (sizeof(g_readPlan) / sizeof(g_readPlan[0]))) {
      g_readPlan[g_readPlanCount++].sensor = s;
    }
  }

  return g_readPlanCount;
}

static void printReadResult(SensorDriver* sensor, bool ok) {
#ifdef RIKA_LEAF_00_ENABLED
  if (sensor == &sensor_leaf_00) {
    if (ok) {
      printer.print(F("[DATA] "), true);
      printer.print(sensor_leaf_00.getSensorId(), true, " | ");
      printer.print(F("Temp="), true);
      printer.print(sensor_leaf_00.leaf_temp, true, " C | ", 1);
      printer.print(F("Hum="), true);
      printer.print(sensor_leaf_00.leaf_humid, true, " %", 1);
      printer.println("", true);
    } else {
      printer.print(F("[DATA] Read fail: "), true);
      printer.print(sensor_leaf_00.getSensorId(), true, " | errors=");
      printer.println((unsigned int)sensor_leaf_00.getConsecutiveErrors(), true);
    }
    return;
  }
#endif

#ifdef RIKA_SOIL3IN1_00_ENABLED
  if (sensor == &sensor_soil_00) {
    if (ok) {
      printer.print(F("[DATA] "), true);
      printer.print(sensor_soil_00.getSensorId(), true, " | ");
      printer.print(F("Temp="), true);
      printer.print(sensor_soil_00.soil_temp, true, " C | ", 1);
      printer.print(F("VWC="), true);
      printer.print(sensor_soil_00.soil_vwc, true, " % | ", 1);
      printer.print(F("EC="), true);
      printer.print(sensor_soil_00.soil_ec, true, " mS/cm", 4);
      printer.println("", true);
    } else {
      printer.print(F("[DATA] Read fail: "), true);
      printer.print(sensor_soil_00.getSensorId(), true, " | errors=");
      printer.println((unsigned int)sensor_soil_00.getConsecutiveErrors(), true);
    }
    return;
  }
#endif
}

static void executeReadPlan() {
  if (g_readPlanCount == 0) return;

  for (uint8_t powerLine = 0; powerLine < PCB_POWERLINE_COUNT; ++powerLine) {
    if (!hasPlannedSensorOnPowerLine(powerLine)) {
      continue;
    }

    printer.println(F("============================================================"), true);
    printer.print(F("[EXEC] PowerLine "), true);
    printer.print((unsigned int)powerLine, true, " | Voltage=");
    printer.print((unsigned int)PCB_POWERLINE_VOLTAGES[powerLine], true, " V");
    printer.println("", true);

    if (!powerLineReadState(powerLine)) {
      printer.println(F("[EXEC] PowerLine is OFF -> enabling"), true);
      powerLineSet(powerLine, true);
    } else {
      printer.println(F("[EXEC] PowerLine already ON"), true);
    }

    const uint32_t warmupMs = getMaxWarmupForPowerLine(powerLine);
    if (warmupMs > 0) {
      printer.print(F("[EXEC] Warm-up on this power line = "), true);
      printer.print((unsigned long)warmupMs, true, " ms");
      printer.println("", true);
      delay(warmupMs);
    }

    for (uint8_t iface = 0; iface < PCB_RS485_PORT_COUNT; ++iface) {
      if (!hasPlannedSensorInGroup(powerLine, iface)) {
        continue;
      }

      printer.print(F("[EXEC] Interface "), true);
      printer.println((unsigned int)iface, true);

      rs485InterfaceSet(iface, true);

      if (PCB_RS485_ENABLE_DELAY_MS[iface] > 0) {
        delay(PCB_RS485_ENABLE_DELAY_MS[iface]);
      }

      for (size_t i = 0; i < g_readPlanCount; ++i) {
        SensorDriver* s = g_readPlan[i].sensor;

        if (s->getPowerLineIndex() != powerLine) continue;
        if (s->getInterfaceIndex() != iface) continue;

        printer.println(F("------------------------------------------------------------"), true);
        printer.print(F("[EXEC] Reading sensor: "), true);
        printer.println(s->getSensorId(), true);

        const bool ok = s->readData();
        printReadResult(s, ok);
      }

      rs485InterfaceSet(iface, false);
    }

    if (!powerLineShouldStayOn(powerLine)) {
      printer.println(F("[EXEC] PowerLine can be turned OFF"), true);
      powerLineSet(powerLine, false);
    } else {
      printer.println(F("[EXEC] PowerLine stays ON due to keepPowerOn policy"), true);
    }

    printer.println(F(""), true);
  }
}

void setup() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);

  printBanner();
  initPowerLines();
  initInterfaces();

#if defined(ARDUINO_ARCH_ESP32)
  rs485Bus0.setDebug(&printer);
  rs485Bus0.begin(RS485Hw0,
                  RS485_DEFAULT_BAUD,
                  PCB_RS485_RX_PINS[RS485_PORT_INDEX_0],
                  PCB_RS485_TX_PINS[RS485_PORT_INDEX_0],
                  RS485_DEFAULT_SERIAL_CONFIG);
  rs485Bus0.setDirectionControl(PCB_RS485_DE_PINS[RS485_PORT_INDEX_0],
                                PCB_RS485_DE_ACTIVE_HIGH[RS485_PORT_INDEX_0]);
#else
  rs485Bus0.setDebug(&printer);
  rs485Bus0.begin(Serial2, RS485_DEFAULT_BAUD, -1, -1, RS485_DEFAULT_SERIAL_CONFIG);
#endif

  printSensorMap();
}

void loop() {
  const uint32_t nowMs = millis();

  buildReadPlan(nowMs);
  printDuePlan();
  executeReadPlan();

  delay(1000);
}