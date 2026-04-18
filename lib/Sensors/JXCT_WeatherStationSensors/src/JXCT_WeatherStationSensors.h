#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"
#include "SensorDriver.h"
#include "Configuration_System.h"

/*
  JXCT_WeatherStationSensors

  Concrete RS485 Modbus RTU drivers for the remaining station sensors whose
  local manuals/protocol notes live in docs/datasheets and docs/protocols.

  Shared communication defaults:
  - Protocol: Modbus RTU
  - Function: 0x03 for reads
  - Device address: usually 0x01 from the manuals
  - Serial: 9600 baud, 8 data bits, no parity, 1 stop bit
  - Address changes, where documented or shield-compatible, use function 0x06
    writing the new address to register 0x0100.

  Multi-sensor shield register maps used here:
  - JXCT_AirQualityShield:
      humidity 0x0000, temperature 0x0001, PM2.5 0x0004,
      TVOC 0x0006, PM10 0x0009.
  - JXBS_GasO3CONH3Shield:
      CO 0x0006, O3 0x0007, NH3 0x0008.
      These are shield registers; standalone O3/NH3 manuals list 0x0006.
  - JXBS_GasSO2NO2PressureShield:
      NO2 0x0006, SO2 0x0007, atmospheric pressure 0x0012/0x0013.
      Local SO2/NO2 PDFs are analog-output manuals, so gas scaling is
      configurable and documented in the example config.
*/

class JXCT_ModbusSensorBase : public SensorDriver {
public:
  bool changeAddress(uint8_t newAddress,
                     uint8_t maxRetries = SENSOR_DEFAULT_BUS_RETRIES,
                     uint16_t readTimeoutMs = 500,
                     uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

  uint8_t scanForAddress(uint8_t startAddr = 1,
                         uint8_t endAddr = 247,
                         uint16_t readTimeoutMs = 150,
                         uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

protected:
  static const uint8_t MAX_READ_REGISTERS = 24;

  JXCT_ModbusSensorBase(RS485Bus& bus,
                        const char* sensorId,
                        uint8_t address,
                        uint16_t scanRegister,
                        bool debugEnable,
                        uint8_t powerLineIndex,
                        uint8_t interfaceIndex,
                        uint16_t sampleRateMin,
                        uint32_t warmUpTimeMs,
                        uint8_t maxConsecutiveErrors,
                        uint32_t minUsefulPowerOffMs);

  bool readRegisterBlock(uint16_t startRegister,
                         uint8_t registerCount,
                         uint16_t* words,
                         uint8_t driverRetries,
                         uint16_t readTimeoutMs,
                         uint16_t afterReqDelayMs);

  bool writeSingleRegisterEcho(uint16_t registerAddress,
                               uint16_t value,
                               bool acceptValueAsAddress,
                               uint8_t maxRetries,
                               uint16_t readTimeoutMs,
                               uint16_t afterReqDelayMs);

  void logParsedU16(const __FlashStringHelper* driverLabel,
                    const __FlashStringHelper* valueLabel,
                    uint16_t raw,
                    double value,
                    const __FlashStringHelper* unit,
                    uint8_t decimals) const;

  static bool isFaultRaw16(uint16_t raw);
  static double scaleU16(uint16_t raw, double divisor);
  static uint32_t joinU32(uint16_t highWord, uint16_t lowWord);

  RS485Bus& _bus;
  uint16_t _scanRegister;
  bool _lastParsedFrame;
};

/*
  JXCT_AirQualityShield

  One RS485 board containing:
  - humidity:    register 0x0000, raw / 10 %RH
  - temperature: register 0x0001, signed raw / 10 C
  - PM2.5:       register 0x0004, raw ug/m3
  - TVOC:        register 0x0006, raw ppb
  - PM10:        register 0x0009, raw ug/m3

  Source notes:
  - RS485 TVOC.pdf
  - JXBS-3001-PM2.5 10 -RS485 - Shutter Box Type.docx
*/
class JXCT_AirQualityShield : public JXCT_ModbusSensorBase {
public:
  double humidity_percent;
  double air_temperature_C;
  uint16_t pm2_5_raw;
  uint16_t pm10_raw;
  uint16_t tvoc_raw_ppb;
  double pm2_5_ug_m3;
  double pm10_ug_m3;
  double tvoc_ppb;
  double tvoc_ppm;

  JXCT_AirQualityShield(RS485Bus& bus,
                        const char* sensorId,
                        uint8_t address,
                        bool debugEnable = false,
                        double maxPM_ug_m3 = 300.0,
                        double maxTVOC_ppb = 1000.0,
                        uint8_t powerLineIndex = 0,
                        uint8_t interfaceIndex = 0,
                        uint16_t sampleRateMin = 1,
                        uint32_t warmUpTimeMs = 500,
                        uint8_t maxConsecutiveErrors = 10,
                        uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readAirQualityBlock(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                           uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                           uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxPM_ug_m3;
  double _maxTVOC_ppb;

  bool validateReadings() const;
};

/*
  JXCT_WindSpeed
  - Register 0x0016, raw / 10 m/s.
  - Source: RS485 Wind Speed Sensor.pdf
*/
class JXCT_WindSpeed : public JXCT_ModbusSensorBase {
public:
  uint16_t wind_speed_raw;
  double wind_speed_m_s;

  JXCT_WindSpeed(RS485Bus& bus,
                 const char* sensorId,
                 uint8_t address,
                 bool debugEnable = false,
                 double maxWindSpeed_m_s = 30.0,
                 uint8_t powerLineIndex = 0,
                 uint8_t interfaceIndex = 0,
                 uint16_t sampleRateMin = 1,
                 uint32_t warmUpTimeMs = 500,
                 uint8_t maxConsecutiveErrors = 10,
                 uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readWindSpeed(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                     uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                     uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxWindSpeed_m_s;
  bool validateWindSpeed() const;
};

/*
  JXCT_WindDirection
  - Register 0x0000, raw degrees 0..360.
  - Source: RS485 Wind Direction Sensor.pdf
*/
class JXCT_WindDirection : public JXCT_ModbusSensorBase {
public:
  uint16_t wind_direction_raw;
  double wind_direction_deg;

  JXCT_WindDirection(RS485Bus& bus,
                     const char* sensorId,
                     uint8_t address,
                     bool debugEnable = false,
                     uint8_t powerLineIndex = 0,
                     uint8_t interfaceIndex = 0,
                     uint16_t sampleRateMin = 1,
                     uint32_t warmUpTimeMs = 500,
                     uint8_t maxConsecutiveErrors = 10,
                     uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readWindDirection(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                         uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                         uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  bool validateWindDirection() const;
};

/*
  JXCT_UVRays
  - Humidity: register 0x0000, raw / 10 %RH.
  - Temperature: register 0x0001, signed raw / 10 C.
  - UV: register 0x0008, raw / 10 W/m2.
  - Source: RS485-UV rays.pdf
*/
class JXCT_UVRays : public JXCT_ModbusSensorBase {
public:
  double humidity_percent;
  double air_temperature_C;
  uint16_t uv_raw;
  double uv_w_m2;

  JXCT_UVRays(RS485Bus& bus,
              const char* sensorId,
              uint8_t address,
              bool debugEnable = false,
              double maxUV_w_m2 = 150.0,
              uint8_t powerLineIndex = 0,
              uint8_t interfaceIndex = 0,
              uint16_t sampleRateMin = 1,
              uint32_t warmUpTimeMs = 500,
              uint8_t maxConsecutiveErrors = 10,
              uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readEnvironment(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                       uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                       uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool readUV(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
              uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
              uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxUV_w_m2;
  bool validateEnvironment() const;
  bool validateUV() const;
};

/*
  JXCT_PAR
  - Register 0x0006, raw value. Local protocol notes use W/m2 bounds, but the
    manual wording is sensor-defined; keep scale configurable in higher layers
    if your exact probe uses another unit.
  - Source: RS485_Photosynthetically_active_radiation_Sensor_User_Manual.pdf
*/
class JXCT_PAR : public JXCT_ModbusSensorBase {
public:
  uint16_t par_raw;
  double par_value;

  JXCT_PAR(RS485Bus& bus,
           const char* sensorId,
           uint8_t address,
           bool debugEnable = false,
           double parScaleDivisor = 1.0,
           double maxPAR = 2000.0,
           uint8_t powerLineIndex = 0,
           uint8_t interfaceIndex = 0,
           uint16_t sampleRateMin = 1,
           uint32_t warmUpTimeMs = 500,
           uint8_t maxConsecutiveErrors = 10,
           uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readPAR(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
               uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
               uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _parScaleDivisor;
  double _maxPAR;
  bool validatePAR() const;
};

/*
  JXCT_TotalSolarRadiation
  - Register 0x0000, raw W/m2.
  - Source: RS485_Total_Solar_Radiation_Sensor_User_Manual.
*/
class JXCT_TotalSolarRadiation : public JXCT_ModbusSensorBase {
public:
  uint16_t solar_raw;
  double total_solar_w_m2;

  JXCT_TotalSolarRadiation(RS485Bus& bus,
                           const char* sensorId,
                           uint8_t address,
                           bool debugEnable = false,
                           double maxSolar_w_m2 = 1500.0,
                           uint8_t powerLineIndex = 0,
                           uint8_t interfaceIndex = 0,
                           uint16_t sampleRateMin = 1,
                           uint32_t warmUpTimeMs = 500,
                           uint8_t maxConsecutiveErrors = 10,
                           uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readSolarRadiation(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                          uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                          uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxSolar_w_m2;
  bool validateSolar() const;
};

/*
  JXCT_Evaporation
  - Register 0x0006, raw evaporation / water-weight value.
  - Register 0x0102, tare/clear command with function 0x06.
  - Source: RS485 Evaporation Sensor Manual - JXCT Manual.pdf
*/
class JXCT_Evaporation : public JXCT_ModbusSensorBase {
public:
  uint16_t evaporation_raw;
  double evaporation_value;

  JXCT_Evaporation(RS485Bus& bus,
                   const char* sensorId,
                   uint8_t address,
                   bool debugEnable = false,
                   double evaporationScaleDivisor = 1.0,
                   double maxEvaporationValue = 200.0,
                   uint8_t powerLineIndex = 0,
                   uint8_t interfaceIndex = 0,
                   uint16_t sampleRateMin = 1,
                   uint32_t warmUpTimeMs = 500,
                   uint8_t maxConsecutiveErrors = 10,
                   uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readEvaporation(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                       uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                       uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool tare(uint16_t commandValue = 0x0001,
            uint8_t maxRetries = SENSOR_DEFAULT_BUS_RETRIES,
            uint16_t readTimeoutMs = 500,
            uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _evaporationScaleDivisor;
  double _maxEvaporationValue;
  bool validateEvaporation() const;
};

/*
  JXBS_GasO3CONH3Shield
  - One shield with CO at 0x0006, O3 at 0x0007, NH3 at 0x0008.
  - O3 scale follows O3 manual convention: raw / 100 ppm.
  - CO and NH3 scale follow gas manual convention: raw / 10 ppm.
*/
class JXBS_GasO3CONH3Shield : public JXCT_ModbusSensorBase {
public:
  uint16_t co_raw;
  uint16_t o3_raw;
  uint16_t nh3_raw;
  double co_ppm;
  double o3_ppm;
  double nh3_ppm;

  JXBS_GasO3CONH3Shield(RS485Bus& bus,
                        const char* sensorId,
                        uint8_t address,
                        bool debugEnable = false,
                        double maxCO_ppm = 2000.0,
                        double maxO3_ppm = 100.0,
                        double maxNH3_ppm = 5000.0,
                        uint8_t powerLineIndex = 0,
                        uint8_t interfaceIndex = 0,
                        uint16_t sampleRateMin = 1,
                        uint32_t warmUpTimeMs = 500,
                        uint8_t maxConsecutiveErrors = 10,
                        uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readGasBlock(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                    uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                    uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _maxCO_ppm;
  double _maxO3_ppm;
  double _maxNH3_ppm;
  bool validateGases() const;
};

/*
  JXBS_GasSO2NO2PressureShield
  - One shield with NO2 at 0x0006, SO2 at 0x0007, pressure at 0x0012/0x0013.
  - Pressure is u32 high-word/low-word, raw / 100 mbar.
  - Local SO2/NO2 PDFs are analog-output manuals, so SO2/NO2 divisors are
    configurable. Defaults use raw / 10 ppm as a JXCT/JXBS gas-family
    convention until hardware confirms otherwise.
*/
class JXBS_GasSO2NO2PressureShield : public JXCT_ModbusSensorBase {
public:
  uint16_t no2_raw;
  uint16_t so2_raw;
  uint32_t pressure_raw;
  double no2_ppm;
  double so2_ppm;
  double pressure_mbar;

  JXBS_GasSO2NO2PressureShield(RS485Bus& bus,
                               const char* sensorId,
                               uint8_t address,
                               bool debugEnable = false,
                               double no2ScaleDivisor = 10.0,
                               double so2ScaleDivisor = 10.0,
                               double maxNO2_ppm = 2000.0,
                               double maxSO2_ppm = 2000.0,
                               uint8_t powerLineIndex = 0,
                               uint8_t interfaceIndex = 0,
                               uint16_t sampleRateMin = 1,
                               uint32_t warmUpTimeMs = 500,
                               uint8_t maxConsecutiveErrors = 10,
                               uint32_t minUsefulPowerOffMs = 60000UL);

  bool readData() override;
  void setFallbackValues() override;
  bool readGasBlock(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                    uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                    uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);
  bool readPressure(uint8_t driverRetries = SENSOR_DEFAULT_DRIVER_RETRIES,
                    uint16_t readTimeoutMs = SENSOR_DEFAULT_READ_TIMEOUT_MS,
                    uint16_t afterReqDelayMs = SENSOR_DEFAULT_AFTER_REQ_MS);

private:
  double _no2ScaleDivisor;
  double _so2ScaleDivisor;
  double _maxNO2_ppm;
  double _maxSO2_ppm;
  bool validateGases() const;
  bool validatePressure() const;
};
