#include "PrintController.h"
#include "RS485Modbus.h"
#include "RikaLeafSensor.h"
#include <Arduino.h>

// -------------------- Compile-time settings --------------------
#define SERIAL_BAUD 115200
#define RS485_BAUD 9600

// Choose correct pins based on board architecture
#if defined(ARDUINO_ARCH_ESP32)
#define RS485_TX_PIN 17
#define RS485_RX_PIN 16
#define RS485_DE_PIN 21
HardwareSerial RS485Serial(1);
#else
// Mega 2560 defaults
#define RS485_DE_PIN 21
#endif
// ----------------------------------------------------------------
#define TOUCH_PIN 4 // Change to your ESP32 touch pin (T0 = GPIO 4)

// Instantiating core transport mechanisms
static PrintController printer(Serial, true, PrintController::TRACE);
static RS485Bus bus;

uint8_t targetAddress = 0x01; // Default sensor address

// Instantiating the Rika Leaf driver
// Update 'targetAddress' if your board is assigned to another ID!
static RikaLeafSensor rikaLeaf(bus, targetAddress);

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  printer.logln(PrintController::SUMMARY,
                F("BOOT: Rika Leaf Sensor Example App Starting..."));

  // Connect Logger to RS485 Bus
  bus.setLogger(&printer);

  // Initialize Bus Hardware
#if defined(ARDUINO_ARCH_ESP32)
  bus.begin(RS485Serial, RS485_BAUD, RS485_RX_PIN, RS485_TX_PIN);
#else
  bus.begin(Serial2, RS485_BAUD); // MEGA2560 example via Serial2
#endif

  // Force direction control for the transmit/receive cycle
  bus.setDirectionControl(RS485_DE_PIN, true);

  printer.logln(PrintController::SUMMARY,
                F("BOOT: Rika Initialized on Address 0x01."));

  /*
    Optional Broadcast Change Address Example:
    UNCOMMENT IF YOU NEED TO CHANGE THE SENSOR ADDRESS
    WARNING: The WHITE wire MUST be connected to the positive power supply
    (+12/24V) WARNING: Only ONE sensor can be on the physical bus when running
    this!
  */
  // delay(2000);
  // printer.logln(PrintController::SUMMARY, F("ATTEMPTING ADDRESS CHANGE TO
  // 0x03...")); if(rikaLeaf.changeAddress(0x03)) { // Change sensor to address
  // 0x03 via Broadcast 0xFF
  //   printer.logln(PrintController::SUMMARY, F("SUCCESS: Address is now
  //   0x03."));
  // } else {
  //   printer.logln(PrintController::SUMMARY, F("FAILED: Could not change Rika
  //   Leaf address."));
  // }
}

void scanAddresses() {
  printer.logln(PrintController::SUMMARY,
                F("\n--- Starting Modbus Address Scan (0-247) ---"));
  for (uint8_t addr = 0; addr <= 247; addr++) {
    uint8_t req[8];
    req[0] = addr;
    req[1] = 0x03; // Read Holding Registers
    req[2] = 0x00;
    req[3] = 0x00; // Start at 0x0000
    req[4] = 0x00;
    req[5] = 0x01; // Read 1 register
    uint16_t crc = RS485Bus::crc16Modbus(req, 6);
    req[6] = crc & 0xFF;
    req[7] = crc >> 8;

    if (bus.transferRaw(req, 8, 100)) { // 100ms timeout
      printer.log(PrintController::SUMMARY, F("Found device at address: 0x"));
      if (addr < 16)
        printer.print(F("0"));
      printer.println(addr, HEX);
    }
  }
  printer.logln(PrintController::SUMMARY, F("--- Scan Complete ---\n"));
  bus.flushInput();
}

void loop() {
#if defined(ARDUINO_ARCH_ESP32)
  // Check if capacitance falls below threshold (touched)
  // Threshold may need adjustment based on your board
  if (touchRead(TOUCH_PIN) < 40) {
    delay(50); // debounce
    if (touchRead(TOUCH_PIN) < 40) {
      scanAddresses();
      delay(1000); // Prevent multiple scans
    }
  }
#endif

  static uint32_t lastPoll = 0;
  if (millis() - lastPoll >= 3000 || lastPoll == 0) {
    lastPoll = millis();

    RikaLeafSensor_Data data{};

    printer.logln(PrintController::SUMMARY, F("Polling Rika Leaf Sensor..."));

    bool ok = rikaLeaf.readAll(data);

    if (ok) {
      printer.logln(PrintController::SUMMARY,
                    F("\n===== RIKA LEAF SENSOR DATA ====="));

      printer.log(PrintController::SUMMARY, F("Temperature (C): "));
      printer.println(data.rika_leaf_temp, 1);

      printer.log(PrintController::SUMMARY, F("Humidity  (%RH): "));
      printer.println(data.rika_leaf_humid, 1);

      printer.logln(PrintController::SUMMARY,
                    F("=================================\n"));
    } else {
      printer.logln(
          PrintController::SUMMARY,
          F("ERROR: Read failed (CRC Error, Timeout, or Bounds Exceeded)."));
    }
  }
}
