#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "SensorDriver.h"
#include "Configuration_System.h"
#include "PrintController.h"

/*
  HondeSHT45

  Driver intent:
  - I2C driver for a Honde air temperature and humidity sensor based on SHT45
    / SHT4x protocol.
  - Primary measurements:
      air_temperature -> degrees Celsius
      air_humidity    -> %RH

  Electrical notes:
  - Sensor power: 3.3 V.
  - I2C bus: SDA/SCL from the active PCB profile.
  - Default SHT4x I2C address: 0x44.

  Measurement command:
  - 0xFD = high precision measurement.
  - Response is 6 bytes:
      temp MSB, temp LSB, temp CRC,
      humidity MSB, humidity LSB, humidity CRC.
  - CRC-8 polynomial: 0x31, initial value: 0xFF.

  Conversion:
  - temperature C = -45 + 175 * rawTemperature / 65535
  - humidity %RH  =  -6 + 125 * rawHumidity    / 65535, clamped to 0..100
*/
class HondeSHT45 : public SensorDriver {
public:
  double air_temperature;
  double air_humidity;

  HondeSHT45(const char* sensorId,
             uint8_t address = 0x44,
             bool debugEnable = false,
             uint8_t powerLineIndex = 0,
             uint8_t interfaceIndex = 0,
             uint16_t sampleRateMin = 1,
             uint32_t warmUpTimeMs = 10,
             uint8_t maxConsecutiveErrors = 10,
             uint32_t minUsefulPowerOffMs = 60000UL);

  void begin(TwoWire* wire = &Wire);
  void setLogger(PrintController* logger);

  bool readData() override;
  void setFallbackValues() override;

  bool readTemperatureHumidity(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                               uint16_t measurementDelayMs = 10);

  bool softReset(uint16_t resetDelayMs = 2);

private:
  TwoWire* _wire;
  PrintController* _log;
  bool _lastParsedFrame;

  bool validateTemperatureHumidity() const;
  bool writeCommand(uint8_t command);
  bool readMeasurementBytes(uint8_t response[6]);
  void logParsed(uint16_t rawTemperature, uint16_t rawHumidity) const;

  static uint8_t crc8(const uint8_t* data, size_t len);
  static bool crcOk(uint8_t msb, uint8_t lsb, uint8_t expectedCrc);

  static const uint8_t COMMAND_MEASURE_HIGH_PRECISION = 0xFD;
  static const uint8_t COMMAND_SOFT_RESET = 0x94;
};
