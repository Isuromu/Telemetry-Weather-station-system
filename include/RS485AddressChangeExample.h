#pragma once
#include <Arduino.h>
#include "PrintController.h"

/*
  Shared helper for RS485 sensor examples.

  Expected config.h macros:
    ADDRESS_CHANGE_AT_BOOT
      false -> no address write is attempted
      true  -> setup() may call driver.changeAddress()

    ADDRESS_CHANGE_NEW_ADDRESS
      target Modbus address to write when ADDRESS_CHANGE_AT_BOOT is true

    ADDRESS_CHANGE_BUTTON_PIN
      optional. If defined, the example waits 5 seconds for this button before
      writing the address. If undefined, no GPIO is configured or read.

    ADDRESS_CHANGE_BUTTON_ACTIVE_STATE
      optional. Defaults to LOW for INPUT_PULLUP service buttons.

    ADDRESS_CHANGE_BUTTON_WAIT_MS
      optional. Defaults to 5000 ms.
*/

#ifndef ADDRESS_CHANGE_BUTTON_ACTIVE_STATE
#define ADDRESS_CHANGE_BUTTON_ACTIVE_STATE LOW
#endif

#ifndef ADDRESS_CHANGE_BUTTON_WAIT_MS
#define ADDRESS_CHANGE_BUTTON_WAIT_MS 5000UL
#endif

static bool rs485AddressChangeButtonPressed() {
#if defined(ADDRESS_CHANGE_BUTTON_PIN)
  pinMode(ADDRESS_CHANGE_BUTTON_PIN, INPUT_PULLUP);

  const unsigned long start = millis();
  while ((millis() - start) < ADDRESS_CHANGE_BUTTON_WAIT_MS) {
    if (digitalRead(ADDRESS_CHANGE_BUTTON_PIN) == ADDRESS_CHANGE_BUTTON_ACTIVE_STATE) {
      delay(50);
      return digitalRead(ADDRESS_CHANGE_BUTTON_PIN) == ADDRESS_CHANGE_BUTTON_ACTIVE_STATE;
    }
    delay(10);
  }

  return false;
#else
  return true;
#endif
}

static void printAddressChangeGate(PrintController& printer) {
#if defined(ADDRESS_CHANGE_BUTTON_PIN)
  printer.print(F("[APP] Address change is armed. Press and hold button on GPIO "), true);
  printer.print((int)ADDRESS_CHANGE_BUTTON_PIN, true, " within ");
  printer.print((unsigned long)(ADDRESS_CHANGE_BUTTON_WAIT_MS / 1000UL), true, " seconds.");
  printer.println("", true);
#else
  printer.println(F("[APP] Address change is armed with no button gate."), true);
  printer.println(F("[APP] Because ADDRESS_CHANGE_BUTTON_PIN is undefined, no GPIO is used."), true);
#endif
}

static void printAddressChangeTarget(PrintController& printer,
                                     uint8_t currentAddress,
                                     uint8_t newAddress,
                                     const __FlashStringHelper* requirementText) {
  printer.println(F("[APP] Address-change request configured at boot."), true);

  printer.print(F("[APP] Current address: 0x"), true);
  printer.print((unsigned int)currentAddress, true, " | ", HEX);
  printer.print(F("New address: 0x"), true);
  printer.println((unsigned int)newAddress, true, "", HEX);

  if (requirementText) {
    printer.print(F("[APP] Required condition: "), true);
    printer.println(requirementText, true);
  }

  printAddressChangeGate(printer);
}

template <typename SensorT>
static void runAddressChangeAtBoot(SensorT& sensor,
                                   PrintController& printer,
                                   uint8_t newAddress,
                                   const __FlashStringHelper* requirementText) {
  printAddressChangeTarget(printer, sensor.getAddress(), newAddress, requirementText);

  if (!rs485AddressChangeButtonPressed()) {
    printer.println(F("[APP] Address-change button was not pressed. Skipping address write."), true);
    printer.println(F(""), true);
    return;
  }

  // This is the only place examples call driver.changeAddress().
  // Keep ADDRESS_CHANGE_AT_BOOT false unless you are intentionally writing
  // the sensor's persistent Modbus address.
  printer.println(F("[APP] Calling changeAddress() now."), true);
  if (sensor.changeAddress(newAddress, SENSOR_DEFAULT_BUS_RETRIES, 500, SENSOR_DEFAULT_AFTER_REQ_MS)) {
    printer.print(F("[APP] Address changed successfully to 0x"), true);
    printer.println((unsigned int)sensor.getAddress(), true, "", HEX);
  } else {
    printer.println(F("[APP] Address change failed."), true);
  }
  printer.println(F(""), true);
}
