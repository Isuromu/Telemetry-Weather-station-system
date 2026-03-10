#include "SlotManager.h"

// ============================================================================
// Constructor
// ============================================================================
SlotManager::SlotManager()
  : _sensorCount(0),
    _max1Min(DEFAULT_MAX_1MIN_SENSORS),
    _max5Min(DEFAULT_MAX_5MIN_SENSORS),
    _log(nullptr), _debugEnable(false) {
  memset(_sensors, 0, sizeof(_sensors));
}

// ============================================================================
// registerSensor — add a sensor to the registry
// ============================================================================
bool SlotManager::registerSensor(SensorDriver* sensor) {
  if (!sensor) return false;
  if (_sensorCount >= MAX_TOTAL_SENSORS) return false;

  // Check slot limits before accepting
  uint8_t rate = sensor->getSampleRateMin();

  if (rate == 1 && countAtRate(1) >= _max1Min) {
    if (_log) {
      _log->println(F("[Slots] REJECTED: 1-min slot full!"), _debugEnable);
    }
    return false;
  }
  if (rate == 5 && countAtRate(5) >= _max5Min) {
    if (_log) {
      _log->println(F("[Slots] REJECTED: 5-min slot full!"), _debugEnable);
    }
    return false;
  }

  _sensors[_sensorCount] = sensor;
  _sensorCount++;

  if (_log) {
    _log->print(F("[Slots] Registered sensor addr=0x"), _debugEnable);
    _log->print(sensor->getAddress(), _debugEnable, "", HEX);
    _log->print(F(" rate="), _debugEnable);
    _log->print((int)rate, _debugEnable);
    _log->println(F(" min"), _debugEnable);
  }

  return true;
}

// ============================================================================
// removeSensor
// ============================================================================
void SlotManager::removeSensor(SensorDriver* sensor) {
  for (uint8_t i = 0; i < _sensorCount; i++) {
    if (_sensors[i] == sensor) {
      // Shift remaining sensors down
      for (uint8_t j = i; j < _sensorCount - 1; j++) {
        _sensors[j] = _sensors[j + 1];
      }
      _sensors[_sensorCount - 1] = nullptr;
      _sensorCount--;
      return;
    }
  }
}

// ============================================================================
// validate — check all slot limits
// ============================================================================
bool SlotManager::validate() {
  if (countAtRate(1) > _max1Min) return false;
  if (countAtRate(5) > _max5Min) return false;
  return true;
}

// ============================================================================
// countAtRate — how many sensors use a specific rate
// ============================================================================
uint8_t SlotManager::countAtRate(uint8_t rateMinutes) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < _sensorCount; i++) {
    if (_sensors[i] && _sensors[i]->getSampleRateMin() == rateMinutes) {
      count++;
    }
  }
  return count;
}

// ============================================================================
// getSensor — get sensor by index
// ============================================================================
SensorDriver* SlotManager::getSensor(uint8_t index) {
  if (index >= _sensorCount) return nullptr;
  return _sensors[index];
}

// ============================================================================
// changeSensorRate — change a sensor's rate with slot validation
// ============================================================================
bool SlotManager::changeSensorRate(SensorDriver* sensor, uint8_t newRateMin) {
  if (!sensor) return false;

  uint8_t oldRate = sensor->getSampleRateMin();

  // Temporarily set the new rate and check if slots are OK
  sensor->setSampleRate(newRateMin);

  if (!validate()) {
    // Revert — slot limit exceeded
    sensor->setSampleRate(oldRate);
    if (_log) {
      _log->println(F("[Slots] Rate change REJECTED — slot full!"), _debugEnable);
    }
    return false;
  }

  if (_log) {
    _log->print(F("[Slots] Sensor 0x"), _debugEnable);
    _log->print(sensor->getAddress(), _debugEnable, "", HEX);
    _log->print(F(" rate changed: "), _debugEnable);
    _log->print((int)oldRate, _debugEnable);
    _log->print(F(" -> "), _debugEnable);
    _log->print((int)newRateMin, _debugEnable);
    _log->println(F(" min"), _debugEnable);
  }

  return true;
}

// ============================================================================
// setDebug
// ============================================================================
void SlotManager::setDebug(PrintController* printer, bool enable) {
  _log = printer;
  _debugEnable = enable;
}

// ============================================================================
// printStatus
// ============================================================================
void SlotManager::printStatus() {
  if (!_log) return;

  _log->println(F("[Slots] --- Sensor Registry ---"), _debugEnable);
  _log->print(F("[Slots] Total: "), _debugEnable);
  _log->println((int)_sensorCount, _debugEnable);
  _log->print(F("[Slots] 1-min: "), _debugEnable);
  _log->print((int)countAtRate(1), _debugEnable);
  _log->print(F("/"), _debugEnable);
  _log->println((int)_max1Min, _debugEnable);
  _log->print(F("[Slots] 5-min: "), _debugEnable);
  _log->print((int)countAtRate(5), _debugEnable);
  _log->print(F("/"), _debugEnable);
  _log->println((int)_max5Min, _debugEnable);

  for (uint8_t i = 0; i < _sensorCount; i++) {
    if (_sensors[i]) {
      _log->print(F("  ["), _debugEnable);
      _log->print((int)i, _debugEnable);
      _log->print(F("] addr=0x"), _debugEnable);
      _log->print(_sensors[i]->getAddress(), _debugEnable, "", HEX);
      _log->print(F(" rate="), _debugEnable);
      _log->print((int)_sensors[i]->getSampleRateMin(), _debugEnable);
      _log->print(F("min status="), _debugEnable);
      switch (_sensors[i]->getStatus()) {
        case SENSOR_ONLINE:  _log->println(F("ONLINE"), _debugEnable); break;
        case SENSOR_ERROR:   _log->println(F("ERROR"), _debugEnable); break;
        case SENSOR_OFFLINE: _log->println(F("OFFLINE"), _debugEnable); break;
      }
    }
  }
}
