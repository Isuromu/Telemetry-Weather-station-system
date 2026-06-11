#include "SimpleDS3231.h"

static const uint8_t DS3231_REG_SECONDS = 0x00;
static const uint8_t DS3231_REG_STATUS = 0x0F;
static const uint8_t DS3231_STATUS_OSF = 0x80;

SimpleDS3231::SimpleDS3231(uint8_t address)
    : _wire(nullptr),
      _address(address) {}

void SimpleDS3231::begin(TwoWire& wire) {
  _wire = &wire;
}

bool SimpleDS3231::read(SimpleDS3231DateTime& out) {
  if (!_wire) return false;

  uint8_t status = 0;
  if (!readStatus(status)) return false;
  if ((status & DS3231_STATUS_OSF) != 0) {
    return false;
  }

  uint8_t data[7] = {0};
  if (!readRegisters(DS3231_REG_SECONDS, data, sizeof(data))) {
    return false;
  }

  out.second = bcdToDec(data[0] & 0x7F);
  out.minute = bcdToDec(data[1] & 0x7F);
  out.hour = bcdToDec(data[2] & 0x3F);
  out.day = bcdToDec(data[4] & 0x3F);
  out.month = bcdToDec(data[5] & 0x1F);
  out.year = 2000U + bcdToDec(data[6]);
  out.epochSeconds = toEpochSeconds(out.year, out.month, out.day, out.hour, out.minute, out.second);

  return isValid(out);
}

bool SimpleDS3231::setFromEpoch(uint32_t epochSeconds) {
  if (!_wire || epochSeconds < 1577836800UL) return false; // 2020-01-01

  SimpleDS3231DateTime value = {};
  if (!fromEpochSeconds(epochSeconds, value)) return false;

  const uint8_t data[7] = {
    decToBcd(value.second),
    decToBcd(value.minute),
    decToBcd(value.hour),
    decToBcd(dayOfWeek(epochSeconds)),
    decToBcd(value.day),
    decToBcd(value.month),
    decToBcd((uint8_t)(value.year - 2000U))
  };

  if (!writeRegisters(DS3231_REG_SECONDS, data, sizeof(data))) {
    return false;
  }
  clearOscillatorStopFlag();
  return true;
}

bool SimpleDS3231::isValid(const SimpleDS3231DateTime& value) {
  if (value.year < 2020 || value.year > 2099) return false;
  if (value.month < 1 || value.month > 12) return false;
  if (value.day < 1 || value.day > 31) return false;
  if (value.hour > 23 || value.minute > 59 || value.second > 59) return false;
  if (value.epochSeconds < 1577836800UL) return false;
  return true;
}

bool SimpleDS3231::formatUtc(uint32_t epochSeconds, char* out, size_t outSize) {
  if (!out || outSize == 0) return false;

  SimpleDS3231DateTime value = {};
  if (!fromEpochSeconds(epochSeconds, value)) {
    out[0] = '\0';
    return false;
  }

  snprintf(out,
           outSize,
           "%04u-%02u-%02u %02u:%02u:%02u",
           (unsigned int)value.year,
           (unsigned int)value.month,
           (unsigned int)value.day,
           (unsigned int)value.hour,
           (unsigned int)value.minute,
           (unsigned int)value.second);
  return true;
}

uint32_t SimpleDS3231::toEpochSeconds(uint16_t year,
                                      uint8_t month,
                                      uint8_t day,
                                      uint8_t hour,
                                      uint8_t minute,
                                      uint8_t second) {
  const int32_t days = daysFromCivil((int32_t)year, month, day);
  if (days < 0) return 0;

  return (uint32_t)days * 86400UL +
         (uint32_t)hour * 3600UL +
         (uint32_t)minute * 60UL +
         (uint32_t)second;
}

bool SimpleDS3231::fromEpochSeconds(uint32_t epochSeconds, SimpleDS3231DateTime& out) {
  if (epochSeconds < 1577836800UL) return false;

  const uint32_t secondsOfDay = epochSeconds % 86400UL;
  const int32_t days = (int32_t)(epochSeconds / 86400UL);

  civilFromDays(days, out.year, out.month, out.day);
  out.hour = (uint8_t)(secondsOfDay / 3600UL);
  out.minute = (uint8_t)((secondsOfDay % 3600UL) / 60UL);
  out.second = (uint8_t)(secondsOfDay % 60UL);
  out.epochSeconds = epochSeconds;

  return isValid(out);
}

bool SimpleDS3231::readRegisters(uint8_t firstRegister, uint8_t* data, uint8_t count) {
  if (!_wire || !data || count == 0) return false;

  _wire->beginTransmission(_address);
  _wire->write(firstRegister);
  if (_wire->endTransmission(false) != 0) return false;

  const uint8_t readCount = _wire->requestFrom((int)_address, (int)count);
  if (readCount != count) return false;

  for (uint8_t i = 0; i < count; ++i) {
    data[i] = _wire->read();
  }
  return true;
}

bool SimpleDS3231::writeRegisters(uint8_t firstRegister, const uint8_t* data, uint8_t count) {
  if (!_wire || !data || count == 0) return false;

  _wire->beginTransmission(_address);
  _wire->write(firstRegister);
  for (uint8_t i = 0; i < count; ++i) {
    _wire->write(data[i]);
  }
  return _wire->endTransmission() == 0;
}

bool SimpleDS3231::readStatus(uint8_t& status) {
  return readRegisters(DS3231_REG_STATUS, &status, 1);
}

bool SimpleDS3231::clearOscillatorStopFlag() {
  uint8_t status = 0;
  if (!readStatus(status)) return false;
  status &= (uint8_t)~DS3231_STATUS_OSF;
  return writeRegisters(DS3231_REG_STATUS, &status, 1);
}

uint8_t SimpleDS3231::bcdToDec(uint8_t value) {
  return (uint8_t)(((value >> 4) * 10U) + (value & 0x0F));
}

uint8_t SimpleDS3231::decToBcd(uint8_t value) {
  return (uint8_t)(((value / 10U) << 4) | (value % 10U));
}

int32_t SimpleDS3231::daysFromCivil(int32_t year, uint8_t month, uint8_t day) {
  year -= month <= 2;
  const int32_t era = (year >= 0 ? year : year - 399) / 400;
  const uint32_t yoe = (uint32_t)(year - era * 400);
  const uint32_t mp = (uint32_t)month + (month > 2 ? (uint32_t)-3 : 9);
  const uint32_t doy = (153U * mp + 2U) / 5U + (uint32_t)day - 1U;
  const uint32_t doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

void SimpleDS3231::civilFromDays(int32_t days, uint16_t& year, uint8_t& month, uint8_t& day) {
  days += 719468;
  const int32_t era = (days >= 0 ? days : days - 146096) / 146097;
  const uint32_t doe = (uint32_t)(days - era * 146097);
  const uint32_t yoe = (doe - doe / 1460U + doe / 36524U - doe / 146096U) / 365U;
  int32_t y = (int32_t)yoe + era * 400;
  const uint32_t doy = doe - (365U * yoe + yoe / 4U - yoe / 100U);
  const uint32_t mp = (5U * doy + 2U) / 153U;
  const uint32_t d = doy - (153U * mp + 2U) / 5U + 1U;
  const uint32_t m = mp + (mp < 10U ? 3U : (uint32_t)-9);
  y += m <= 2U;

  year = (uint16_t)y;
  month = (uint8_t)m;
  day = (uint8_t)d;
}

uint8_t SimpleDS3231::dayOfWeek(uint32_t epochSeconds) {
  const uint32_t days = epochSeconds / 86400UL;
  return (uint8_t)(((days + 4UL) % 7UL) + 1UL); // 1970-01-01 was Thursday.
}
