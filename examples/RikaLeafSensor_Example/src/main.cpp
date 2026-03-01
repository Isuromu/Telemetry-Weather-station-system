#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaLeafSensor.h"
#include "config.h"
#include <Arduino.h>

// All settings live in config.h - do NOT hard-code values here.

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial RS485Serial(1);
#endif

static PrintController
    printer(Serial, static_cast<bool>(PRINT_ENABLED),
            static_cast<PrintController::Verbosity>(PRINT_VERBOSITY));
static constexpr PrintController::Verbosity LOG_LEVEL =
    static_cast<PrintController::Verbosity>(PRINT_VERBOSITY);

static RS485Bus bus;
static uint8_t targetAddress = SENSOR_DEFAULT_ADDRESS;
static RikaLeafSensor rikaLeaf(bus, targetAddress);

void scanAddresses() {
  uint16_t scanStart = static_cast<uint16_t>(SCAN_ADDRESS_MIN);
  uint16_t scanEnd = static_cast<uint16_t>(SCAN_ADDRESS_MAX);

  // Bounds clamping
  if (scanStart > 252)
    scanStart = 252;
  if (scanEnd > 252)
    scanEnd = 252;
  if (scanStart > scanEnd) {
    uint16_t t = scanStart;
    scanStart = scanEnd;
    scanEnd = t;
  }

  printer.logln(LOG_LEVEL,
                F("\n=================================================="));
  printer.logln(LOG_LEVEL, F("  MODBUS ADDRESS SCANNER (Configured Range)"));
  printer.log(LOG_LEVEL, F("  Range: 0x"));
  if (scanStart < 16)
    printer.print(F("0"));
  printer.print(scanStart, HEX);
  printer.print(F(" to 0x"));
  if (scanEnd < 16)
    printer.print(F("0"));
  printer.println(scanEnd, HEX);
  printer.logln(LOG_LEVEL,
                F("==================================================\n"));

  uint16_t attempted = 0;
  uint16_t found = 0;
  for (uint16_t addr = scanStart; addr <= scanEnd; ++addr) {
    attempted++;
    // 1. Prepare Request (FC03 Read 2 Registers from 0x0020 per Rika manual)
    uint8_t req[8];
    req[0] = static_cast<uint8_t>(addr);
    req[1] = 0x03;
    req[2] = 0x00;
    req[3] = 0x20;
    req[4] = 0x00;
    req[5] = 0x02;
    uint16_t crc = RS485Bus::crc16Modbus(req, 6);
    req[6] = static_cast<uint8_t>(crc & 0xFF);
    req[7] = static_cast<uint8_t>(crc >> 8);

    // 2. Clear progress line and show what we are doing
    printer.log(LOG_LEVEL, F("[Scanner] Trying 0x"));
    if (addr < 16)
      printer.print(F("0"));
    printer.print(addr, HEX);
    printer.print(F(" REQ=["));
    // Print full request as hex bytes: 0xAA 0xBB ...
    for (int i = 0; i < 8; i++) {
      printer.print(F("0x"));
      if (req[i] < 16) {
        printer.print(F("0"));
      }
      printer.print(req[i], HEX);
      if (i < 7) {
        printer.print(F(" "));
      }
    }
    printer.print(F("] -> "));

    // 3. Attempt Transfer
    if (bus.transferRaw(req, 8, SCAN_TIMEOUT_MS)) {
      printer.println(F("FOUND!"));
      found++;
    } else {
      printer.println(F("no response"));
    }

    // 4. Yield/Delay to keep ESP32 watchdog and background tasks happy
    delay(1);
    yield();
  }

  printer.logln(LOG_LEVEL,
                F("\n--------------------------------------------------"));
  printer.log(LOG_LEVEL, F("Attempted "));
  printer.print(attempted);
  printer.println(F(" addresses."));
  printer.log(LOG_LEVEL, F("Scan Complete. Found "));
  printer.print(found);
  printer.println(F(" devices."));
  printer.logln(LOG_LEVEL,
                F("--------------------------------------------------\n"));
  bus.flushInput();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  printer.println(F(""));
  printer.println(F("==========================================="));
  printer.println(F("  Program for testing sensor alone."));
  printer.println(F("  Rika Leaf Sensor test firmware."));
  printer.println(F("==========================================="));

  pinMode(SCAN_BUTTON_PIN, INPUT_PULLUP);
  bus.setLogger(&printer);

#if defined(ARDUINO_ARCH_ESP32)
  bus.begin(RS485Serial, RS485_BAUD, RS485_RX_PIN, RS485_TX_PIN);
#else
  bus.begin(Serial2, RS485_BAUD);
#endif

  bus.setDirectionControl(RS485_DE_PIN, true);

  printer.logln(
      LOG_LEVEL,
      F("Press SCAN button to enter scanning mode (wait 5 seconds)..."));

  bool enteredScan = false;
  uint32_t deadline = millis() + static_cast<uint32_t>(SCAN_WINDOW_MS);
  while (millis() < deadline) {
    if (digitalRead(SCAN_BUTTON_PIN) == LOW) {
      delay(50);
      if (digitalRead(SCAN_BUTTON_PIN) == LOW) {
        enteredScan = true;
        break;
      }
    }
    delay(10);
  }

  if (enteredScan) {
    printer.logln(LOG_LEVEL, F("Scanning mode entered."));
    scanAddresses();
    printer.logln(LOG_LEVEL, F("Scan done. Continuing normal polling.\n"));
  } else {
    printer.logln(
        LOG_LEVEL,
        F("Button not pressed within 5 seconds. Normal operation.\n"));
  }

  rikaLeaf.setAddress(targetAddress);
  printer.log(LOG_LEVEL, F("Normal mode poll address: 0x"));
  if (targetAddress < 16) {
    printer.print(F("0"));
  }
  printer.println(targetAddress, HEX);
}

void loop() {
  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= static_cast<uint32_t>(POLL_INTERVAL_MS) ||
      lastPoll == 0) {
    lastPoll = millis();

    // Always follow the current address variable configured for normal polling.
    rikaLeaf.setAddress(targetAddress);

    RikaLeafSensor_Data data{};
    printer.logln(LOG_LEVEL, F("Polling Rika Leaf Sensor..."));

    if (rikaLeaf.readAll(data)) {
      printer.logln(LOG_LEVEL, F("\n===== RIKA LEAF SENSOR DATA ====="));
      printer.log(LOG_LEVEL, F("Temperature (C): "));
      printer.println(data.rika_leaf_temp, 1);
      printer.log(LOG_LEVEL, F("Humidity  (%RH): "));
      printer.println(data.rika_leaf_humid, 1);
      printer.logln(LOG_LEVEL, F("=================================\n"));
    } else {
      printer.logln(
          LOG_LEVEL,
          F("ERROR: Read failed (CRC Error, Timeout, or Bounds Exceeded)."));
    }
  }
}
