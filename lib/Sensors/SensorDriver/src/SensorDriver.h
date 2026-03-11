#pragma once
#include <Arduino.h>

/*
  SensorDriver.h

  Transport-neutral base class for logical sensor objects.

  This class stores:
  - station-visible sensor ID
  - bus address
  - debug enable state
  - power line index
  - interface index
  - sample rate
  - warm-up time
  - last read timestamp
  - keepPowerOn policy result
  - sensor health state
*/

enum SensorStatus {
  SENSOR_ONLINE,
  SENSOR_ERROR,
  SENSOR_OFFLINE
};

class SensorDriver {
public:
  SensorDriver(const char* sensorId,
               uint8_t address,
               bool debugEnable,
               uint8_t powerLineIndex,
               uint8_t interfaceIndex,
               uint16_t sampleRateMin,
               uint32_t warmUpTimeMs,
               uint8_t maxConsecutiveErrors = 10,
               uint32_t minUsefulPowerOffMs = 60000UL)
      : _sensorId(sensorId),
        _address(address),
        _debugEnable(debugEnable),
        _dataStatus(false),
        _powerLineIndex(powerLineIndex),
        _interfaceIndex(interfaceIndex),
        _keepPowerOn(false),
        _sampleRateMin(sampleRateMin),
        _warmUpTimeMs(warmUpTimeMs),
        _lastReadTime(0),
        _status(SENSOR_ONLINE),
        _consecutiveErrors(0),
        _maxConsecutiveErrors(maxConsecutiveErrors),
        _minUsefulPowerOffMs(minUsefulPowerOffMs) {
    recalculateKeepPowerOn();
  }

  virtual ~SensorDriver() {}

  const char* getSensorId() const { return _sensorId; }
  void setSensorId(const char* sensorId) { _sensorId = sensorId; }

  uint8_t getAddress() const { return _address; }
  virtual void setAddress(uint8_t address) { _address = address; }

  bool getDebug() const { return _debugEnable; }
  void setDebug(bool enable) { _debugEnable = enable; }

  bool getDataStatus() const { return _dataStatus; }

  uint8_t getPowerLineIndex() const { return _powerLineIndex; }
  uint8_t getInterfaceIndex() const { return _interfaceIndex; }

  bool shouldKeepPowerOn() const { return _keepPowerOn; }

  uint16_t getSampleRateMin() const { return _sampleRateMin; }
  void setSampleRate(uint16_t minutes) {
    _sampleRateMin = minutes;
    recalculateKeepPowerOn();
  }

  uint32_t getWarmUpTimeMs() const { return _warmUpTimeMs; }
  void setWarmUpTimeMs(uint32_t warmUpTimeMs) {
    _warmUpTimeMs = warmUpTimeMs;
    recalculateKeepPowerOn();
  }

  uint32_t getRequestRateMs() const { return (uint32_t)_sampleRateMin * 60000UL; }
  uint32_t getLastReadTime() const { return _lastReadTime; }

  uint32_t getMinUsefulPowerOffMs() const { return _minUsefulPowerOffMs; }
  void setMinUsefulPowerOffMs(uint32_t value) {
    _minUsefulPowerOffMs = value;
    recalculateKeepPowerOn();
  }

  bool isDueForRead(uint32_t nowMs) const {
    if (_lastReadTime == 0) {
      return true;
    }
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
  void recalculateKeepPowerOn() {
    const uint32_t sampleRateMs = getRequestRateMs();

    if (_warmUpTimeMs >= sampleRateMs) {
      _keepPowerOn = true;
      return;
    }

    const uint32_t offWindowMs = sampleRateMs - _warmUpTimeMs;
    _keepPowerOn = (offWindowMs < _minUsefulPowerOffMs);
  }

  const char* _sensorId;
  uint8_t _address;
  bool _debugEnable;
  bool _dataStatus;

  uint8_t _powerLineIndex;
  uint8_t _interfaceIndex;
  bool _keepPowerOn;

  uint16_t _sampleRateMin;
  uint32_t _warmUpTimeMs;
  uint32_t _lastReadTime;

  SensorStatus _status;
  uint8_t _consecutiveErrors;
  uint8_t _maxConsecutiveErrors;
  uint32_t _minUsefulPowerOffMs;
};