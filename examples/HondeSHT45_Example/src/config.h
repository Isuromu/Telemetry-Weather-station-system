#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"

/*
  HondeSHT45 Example - local example config

  Sensor:
  - Honde SHT45 / SHT4x I2C air temperature and humidity sensor

  Electrical notes:
  - Power from 3.3 V only.
  - Connect SDA/SCL to the active PCB I2C pins:
      ESP32-S3 V2: SDA=PCB_I2C_SDA_PIN, SCL=PCB_I2C_SCL_PIN
      Mega2560: hardware SDA/SCL pins from the PCB profile
  - Default SHT4x I2C address is 0x44. Some boards can use 0x45.

  Protocol notes:
  - High precision measurement command: 0xFD
  - Response: temperature word + CRC, humidity word + CRC
  - CRC-8 polynomial 0x31, initial value 0xFF
*/

#define SENSOR_ID                   "honde_sht45_00"
#define SENSOR_I2C_ADDRESS          0x44
#define SENSOR_DEBUG                true

// Optional soft reset during setup before the first read.
#define DO_SOFT_RESET_ON_BOOT       false

// Delay between loop() polling cycles.
#define POLL_INTERVAL_MS            2000
