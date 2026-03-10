#pragma once
#include <Arduino.h>

/*
  SensorDriver - transport-neutral abstract base class.

  This base class represents one logical sensor object.
  It keeps only sensor-level information that main code needs
  for scheduling, power grouping and health tracking.

  Those belong to the main station logic.
*/

enum SensorStatus {
  SENSOR_ONLINE,
  SENSOR_ERROR,
  SENSOR_OFFLINE
};

class SensorDriver {
public:
  static const uint8_t PIN_UNUSED = 0xFF;

  SensorDriver(const char* sensorId = "sensor",
               uint8_t address = 0,
               bool debugEnable = false,
               uint8_t powerPin = PIN_UNUSED,
               uint8_t enablePin = PIN_UNUSED,
               uint16_t sampleRateMin = 1,
               uint32_t warmUpTimeMs = 500,
               uint8_t maxConsecutiveErrors = 10)
      : _sensorId(sensorId),
        _address(address),
        _debugEnable(debugEnable),
        _dataStatus(false),
        _powerPin(powerPin),
        _enablePin(enablePin),
        _keepPowerOn(false),
        _sampleRateMin(sampleRateMin),
        _warmUpTimeMs(warmUpTimeMs),
        _lastReadTime(0),
        _status(SENSOR_ONLINE),
        _consecutiveErrors(0),
        _maxConsecutiveErrors(maxConsecutiveErrors) {
    updateKeepPowerOnRule();
  }

  virtual ~SensorDriver() {}

  const char* getSensorId() const { return _sensorId; }
  void setSensorId(const char* sensorId) { _sensorId = sensorId; }

  uint8_t getAddress() const { return _address; }
  virtual void setAddress(uint8_t address) { _address = address; }

  bool getDebug() const { return _debugEnable; }
  void setDebug(bool enable) { _debugEnable = enable; }

  bool getDataStatus() const { return _dataStatus; }

  uint8_t getPowerPin() const { return _powerPin; }
  uint8_t getEnablePin() const { return _enablePin; }

  bool shouldKeepPowerOn() const { return _keepPowerOn; }
  void setKeepPowerOn(bool keepPowerOn) { _keepPowerOn = keepPowerOn; }

  uint16_t getSampleRateMin() const { return _sampleRateMin; }
  void setSampleRate(uint16_t minutes) {
    _sampleRateMin = minutes;
    updateKeepPowerOnRule();
  }

  uint32_t getWarmUpTimeMs() const { return _warmUpTimeMs; }
  void setWarmUpTimeMs(uint32_t warmUpTimeMs) {
    _warmUpTimeMs = warmUpTimeMs;
    updateKeepPowerOnRule();
  }

  uint32_t getRequestRateMs() const { return (uint32_t)_sampleRateMin * 60000UL; }
  uint32_t getLastReadTime() const { return _lastReadTime; }

  bool isDueForRead(uint32_t nowMs) const {
    return (nowMs - _lastReadTime) >= getRequestRateMs();
  }

  void markReadTime(uint32_t nowMs) { _lastReadTime = nowMs; }

  SensorStatus getStatus() const { return _status; }
  bool isOnline() const { return _status != SENSOR_OFFLINE; }

  uint8_t getConsecutiveErrors() const { return _consecutiveErrors; }
  uint8_t getMaxConsecutiveErrors() const { return _maxConsecutiveErrors; }
  void setMaxConsecutiveErrors(uint8_t limit) { _maxConsecutiveErrors = limit; }

  void markSuccess() {
    _dataStatus = true;
    _consecutiveErrors = 0;
    _status = SENSOR_ONLINE;
  }

  void markFailure() {
    _dataStatus = false;
    if (_consecutiveErrors < 255) {
      ++_consecutiveErrors;
    }

    if (_consecutiveErrors >= _maxConsecutiveErrors) {
      _status = SENSOR_OFFLINE;
    } else {
      _status = SENSOR_ERROR;
    }
  }

  void resetStatus() {
    _dataStatus = false;
    _consecutiveErrors = 0;
    _status = SENSOR_ONLINE;
  }

  virtual bool readData() = 0;
  virtual void setFallbackValues() = 0;

protected:
  void updateKeepPowerOnRule() {
    const uint32_t sampleRateMs = getRequestRateMs();
    _keepPowerOn = (_warmUpTimeMs >= sampleRateMs);
  }

  const char* _sensorId;
  uint8_t _address;
  bool _debugEnable;
  bool _dataStatus;

  uint8_t _powerPin;
  uint8_t _enablePin;
  bool _keepPowerOn;

  uint16_t _sampleRateMin;
  uint32_t _warmUpTimeMs;
  uint32_t _lastReadTime;

  SensorStatus _status;
  uint8_t _consecutiveErrors;
  uint8_t _maxConsecutiveErrors;
};