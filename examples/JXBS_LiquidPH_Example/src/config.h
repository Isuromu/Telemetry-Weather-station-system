#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  JXBS_LiquidPH Example - local example config

  Sensor:
  - JXBS-3001-PH-RS liquid pH sensor
  - RS485 Modbus RTU

  Manual protocol notes:
  - Default serial format: 9600 baud, 8 data bits, no parity, 1 stop bit.
  - Baud options in manual: 2400 / 4800 / 9600.
  - Default Modbus address is usually 0x01.
  - Register 0x0001 = liquid temperature, raw / 10 C.
  - Register 0x0002 = pH, raw / 100 pH.
  - Register 0x0100 = device address.
  - Register 0x0101 = baud rate, not used by this example.

    WIRING FOR THIS DELIVERY
  - Red    -> +12 V DC
  - Black  -> GND / 0 V
  - Yellow -> RS485 A
  - Green  -> RS485 B

  Client Handling, Storage, and Measurement Guide

  PURPOSE
  - This sensor measures liquid pH and liquid temperature.
  - It is designed for direct immersion of the sensing tip into the liquid sample.

  BEFORE USE
  - Verify wiring, polarity, and RS485 A/B lines before power-on.
  - Make sure the probe tip is clean, intact, and still wet.
  - Remove the protective cap carefully.
  - If needed, gently rinse the sensing tip with clean water before measurement.

  HOW TO HOLD AND HANDLE
  - Hold the probe by its body or by the cable area near the body.
  - Do not press, scratch, hit, or rub the sensing tip.
  - Do not carry or swing the sensor by the cable.
  - Always transport the probe with the protective cap installed.

  HOW TO MEASURE
  - Immerse only the sensing end of the probe into the liquid.
  - Do not immerse the transmitter box, cable gland, or unprotected connections.
  - Make sure the sensing tip is fully in contact with the sample liquid.
  - Keep the probe steady during measurement.
  - Wait for the reading to stabilize before recording the result.

  AFTER MEASUREMENT
  - Remove the probe carefully from the liquid.
  - Rinse the sensing tip with clean water.
  - Do not wipe the sensing tip aggressively and do not use abrasive materials.
  - Install the protective cap immediately after use.

  STORAGE
  - Store the probe only with the protective cap installed.
  - Keep the sensing tip wet during storage.
  - Do not allow the pH electrode to dry out.
  - Store in a clean place away from direct sunlight, overheating, and mechanical damage.
  - If the process is stopped, keep the probe either immersed in the measured liquid
    or protected with its storage cap and protective liquid.

  DO NOT
  - Do not store the probe dry.
  - Do not hit, drop, scratch, or squeeze the sensing tip.
  - Do not pull the sensor by the cable.
  - Do not reverse power polarity.
  - Do not swap RS485 A/B lines without verification.
  - Do not leave the probe unprotected during storage or transport.

  IF READINGS ARE UNSTABLE
  - Check 12 V power, polarity, RS485 A/B wiring, and probe cleanliness.
  - Inspect the sensing tip for cracks, chips, or other visible damage.
  - Do not use the probe if the sensing end is mechanically damaged.

  IMPORTANT
  - Keep the pH electrode wet.
  - Always use the protective cap after operation.
*/

#define SENSOR_ID                   "jxbs_liquid_ph_00"
#define SENSOR_ADDRESS              0x03
#define SENSOR_DEBUG                true

// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                     false

// If true, setup() calls changeAddress() once near boot.
// JXBS pH address change writes register 0x0100. Keep only the target sensor
// connected while changing addresses. No broadcast address is documented in
// the available manual, so assume you must know the current address.
#define ADDRESS_CHANGE_AT_BOOT      true
// Target Modbus address written when ADDRESS_CHANGE_AT_BOOT is true.
#define ADDRESS_CHANGE_NEW_ADDRESS  0x50
// Optional safety gate. Uncomment to wait up to 5 seconds for this button.
// If this macro is undefined, no button GPIO is configured or read.
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
// Delay before the actual persistent address write. Gives time to open Serial
// Monitor after reset/upload and see the changeAddress() call.
#define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

// Optional diagnostic reads after the main combined read.
#define READ_TEMPERATURE_ONLY_IN_LOOP false
#define READ_PH_ONLY_IN_LOOP          false

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS            2000
