#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "PrintController.h"
#include "GenericRainGauge.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "esp_sleep.h"
HardwareSerial& DebugPort = Serial0;
RTC_DATA_ATTR bool rainGaugeStarted = false;
#else
#define DebugPort Serial
#endif

#if defined(ARDUINO_ARCH_AVR)
#include <avr/sleep.h>
#endif

static PrintController printer(DebugPort, false);

static GenericRainGauge rain(
    SENSOR_ID,
    SENSOR_DEBUG,
    POWERLINE_INDEX_0,
    0,
    SAMPLE_RATE_1_MIN,
    0,
    SENSOR_DEFAULT_MAX_ERRORS,
    MIN_USEFUL_POWER_OFF_MS,
    (RainGaugeCounter::CountingMode)RAIN_COUNTING_MODE,
    RAIN_PCF8574_ADDRESS,
    RAIN_COUNTER_RESET_PIN,
    RAIN_BYPASS_INTERRUPT_PIN,
    RAIN_INTERRUPT_DEBOUNCE_MS);

static unsigned long sampleTimerMs = 0;

static void beginI2C() {
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(PCB_I2C_SDA_PIN, PCB_I2C_SCL_PIN);
#else
  Wire.begin();
#endif
}

static void sleepBriefly() {
#if defined(ARDUINO_ARCH_AVR)
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_mode();
  sleep_disable();
#else
  delay(50);
#endif
}

#if defined(ARDUINO_ARCH_ESP32)
static void deepSleepUntilNextSample() {
  DebugPort.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)RAIN_SAMPLE_INTERVAL_MS * 1000ULL);
  esp_deep_sleep_start();
}
#endif

static void printBanner() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" Generic Rain Gauge Diagnostic Example"), true);
  printer.println(F("============================================================"), true);
  printer.print(F("[APP] PCB: "), true);
  printer.println(PCB_NAME, true);
  printer.print(F("[APP] Counting mode: "), true);
  printer.println(RainGaugeCounter::countingModeToString((RainGaugeCounter::CountingMode)RAIN_COUNTING_MODE), true);
  printer.println(F("[APP] Pulse factor: 0.2794 mm/pulse"), true);
  printer.println(F("[APP] Rain sensor started counting."), true);
  printer.println(F(""), true);
}

static void printReadResult(bool ok) {
  if (ok) {
    printer.print(F("[APP] Rain pulses: "), true);
    printer.print((unsigned long)rain.pulse_count, true, " | rainfall: ");
    printer.print(rain.rainfall_mm, true, " mm", 4);
    printer.println("", true);
  } else {
    printer.println(F("[APP] Rain read failed."), true);
  }
}

static void sampleAndReset() {
  if (RAIN_COUNTING_MODE == RAIN_GAUGE_COUNTING_MODE_MCU_INTERRUPT) {
    rain.deactivateCounting();
  }

  const bool ok = rain.readData();
  printReadResult(ok);

  if (rain.resetCounter()) {
    printer.println(F("[APP] Rain counter reset."), true);
  } else {
    printer.println(F("[APP] Rain counter reset failed."), true);
  }

  if (RAIN_COUNTING_MODE == RAIN_GAUGE_COUNTING_MODE_MCU_INTERRUPT) {
    rain.activateCounting();
  }
}

void setup() {
  DebugPort.begin(PCB_DEBUG_SERIAL_BAUD);
  delay(300);
  printBanner();

  beginI2C();
  rain.setLogger(&printer);
  rain.begin(&Wire, nullptr);

  if (RAIN_COUNTING_MODE == RAIN_GAUGE_COUNTING_MODE_MCU_INTERRUPT) {
    rain.resetCounter();
    rain.activateCounting();
    sampleTimerMs = millis();
    return;
  }

#if defined(ARDUINO_ARCH_ESP32)
  if (rainGaugeStarted) {
    sampleAndReset();
  } else {
    rain.resetCounter();
    rainGaugeStarted = true;
  }
  deepSleepUntilNextSample();
#else
  rain.resetCounter();
  sampleTimerMs = millis();
#endif
}

void loop() {
  if ((millis() - sampleTimerMs) >= RAIN_SAMPLE_INTERVAL_MS) {
    sampleAndReset();
    sampleTimerMs = millis();
  }
  sleepBriefly();
}
