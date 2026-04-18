#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  HondeSolarRadiation Example - local example config

  Sensor:
  - Honde RD-TSRS2L global solar radiation sensor / pyranometer
  - RS485 Modbus RTU output

  Manual protocol notes:
  - Default serial format: 9600 baud, 8 data bits, no parity, 1 stop bit.
  - Default Modbus address is 0x01.
  - Read solar radiation with function 0x03:
      register 0x0000, count 1
      raw value = W/m2
  - Address change uses function 0x10:
      register 0x0501 = communication address
  - Baud-rate register is 0x0503, not used by this example.

  RS485 wiring from the manual:
  - Red    -> power +
  - Black  -> GND
  - Yellow -> RS485-A
  - Green  -> RS485-B

  Installation notes:
  - Install where the sky view is clear and shadows are avoided.
  - For horizontal mounting, use the level bubble and adjustment feet.
  - The terminal/cable side should face north.
  - Keep the glass dome clean; use a soft cloth with water or alcohol.
  - Recommended recalibration cycle is every two years.
*/

#define SENSOR_ID                   "honde_solar_00"
#define SENSOR_ADDRESS              0x01
#define SENSOR_DEBUG                false

// Scan address range on boot and print first responsive sensor.
#define DO_SCAN                     false

// If true, setup() calls changeAddress() once near boot.
// Honde solar radiation address change writes register 0x0501 with function
// 0x10. Keep only the target sensor connected while changing addresses.
#define ADDRESS_CHANGE_AT_BOOT      false
// Target Modbus address written when ADDRESS_CHANGE_AT_BOOT is true.
#define ADDRESS_CHANGE_NEW_ADDRESS  0x02
// Optional safety gate. Uncomment to wait up to 5 seconds for this button.
// If this macro is undefined, no button GPIO is configured or read.
// #define ADDRESS_CHANGE_BUTTON_PIN PCB_SERVICE_BUTTON_PIN
// Delay before the actual persistent address write. Gives time to open Serial
// Monitor after reset/upload and see the changeAddress() call.
// #define ADDRESS_CHANGE_CALL_DELAY_MS 5000UL

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS            2000
