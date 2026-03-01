#include "PrintController.h"
#include "RS485Modbus.h"
#include <Arduino.h>

// Initialize the printer with the global Serial instance
PrintController printer(Serial, true, PrintController::TRACE);
RS485Bus bus;

#if defined(ARDUINO_ARCH_ESP32)
HardwareSerial RS485Serial(1);
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  printer.println(F("\n=== Comm Libs Smoke Test ==="));

  // Initialize the bus and pass the logger
  bus.setLogger(&printer);

#if defined(ARDUINO_ARCH_ESP32)
  // ESP32: custom pins rx=16, tx=17
  bus.begin(RS485Serial, 9600, 16, 17);
  bus.setDirectionControl(21, true);
#else
  // AVR: Serial2
  bus.begin(Serial2, 9600);
  bus.setDirectionControl(21, true);
#endif

  printer.logln(PrintController::SUMMARY, F("Initialization complete."));
}

void loop() {
  // Simple test loop sending a dummy frame
  delay(2000);

  printer.logln(PrintController::SUMMARY, F("\nSending dummy request..."));
  uint8_t dummyTx[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};

  // Fake prefix and expected length to test the frame matching
  uint8_t prefix[] = {0x01, 0x03, 0x02};
  uint8_t outFrame[16];

  // This will fail since no real device is connected, but it will compile and
  // exercise the APIs
  bool ok = bus.transferAndExtractFixedFrame(dummyTx, sizeof(dummyTx), prefix,
                                             sizeof(prefix),
                                             7, // expected len
                                             outFrame, sizeof(outFrame),
                                             200, // overall timeout
                                             25   // interbyte timeout
  );

  if (!ok) {
    printer.logln(PrintController::SUMMARY,
                  F("No valid response received (Expected in smoke test "
                    "without hardware)."));
  }
}
