#include "RS485Modbus.h"
#include <string.h>

RS485Bus::RS485Bus()
    : _serial(nullptr),
      _log(nullptr),
      _dir(-1, true),
      _preTxDelayUs(200),
      _postTxDelayUs(200),
      _rxLen(0),
      _lastFrameOffset((size_t)-1) {
  memset(_rxBuf, 0, sizeof(_rxBuf));
}

void RS485Bus::begin(HardwareSerial &serial,
                     uint32_t baud,
                     int8_t rxPin,
                     int8_t txPin,
                     uint32_t config) {
  _serial = &serial;

#if defined(ARDUINO_ARCH_ESP32)
  serial.setRxBufferSize((int)(RX_BUFFER_SIZE + 64));
  serial.begin(baud, config, rxPin, txPin);
#else
  (void)rxPin;
  (void)txPin;
  serial.begin(baud, config);
#endif

  _dir.begin();
  flushInput();
}

void RS485Bus::setDirectionControl(int8_t dePin, bool activeHighTX) {
  _dir = DirectionControl(dePin, activeHighTX);
  _dir.begin();
}

void RS485Bus::setDebug(PrintController *logger) {
  _log = logger;
}

void RS485Bus::setTimings(uint16_t preTxDelayUs, uint16_t postTxDelayUs) {
  _preTxDelayUs = preTxDelayUs;
  _postTxDelayUs = postTxDelayUs;
}

void RS485Bus::flushInput() {
  if (!_serial) return;
  while (_serial->available() > 0) {
    (void)_serial->read();
    delay(1);
  }
}

void RS485Bus::CRC_Calc(uint8_t array[], size_t arraySize, bool debug) {
  if (!array || arraySize < 2) return;

  logPrintln(F("[RS485] CRC_Calc() called"), debug);
  logBufferMatrix(array, arraySize, "Request before CRC", debug);

  const uint16_t crc = crc16Modbus(array, arraySize - 2);
  array[arraySize - 2] = (uint8_t)(crc & 0xFF);
  array[arraySize - 1] = (uint8_t)((crc >> 8) & 0xFF);

  logPrint(F("[RS485] Calculated CRC = 0x"), debug);
  logPrintHex(crc, debug);
  logNewLine(debug);

  logBufferMatrix(array, arraySize, "Request after CRC", debug);
}

void RS485Bus::Request_RS485(const uint8_t reqArray[],
                             size_t reqSize,
                             uint16_t afterReqDelayMs,
                             bool debug) {
  if (!_serial || !reqArray || reqSize == 0) return;

  logPrintln(F("[RS485] Request_RS485() called"), debug);
  logPrint(F("[RS485] TX size = "), debug);
  logPrintDec((unsigned long)reqSize, debug);
  logNewLine(debug);

  logPrint(F("[RS485] preTX delay us = "), debug);
  logPrintDec((unsigned long)_preTxDelayUs, debug);
  logNewLine(debug);

  logPrint(F("[RS485] postTX delay us = "), debug);
  logPrintDec((unsigned long)_postTxDelayUs, debug);
  logNewLine(debug);

  logPrint(F("[RS485] after request delay ms = "), debug);
  logPrintDec((unsigned long)afterReqDelayMs, debug);
  logNewLine(debug);

  logBufferMatrix(reqArray, reqSize, "TX request buffer", debug);

  flushInput();
  _dir.setTX(true);

  if (_preTxDelayUs > 0) {
    delayMicroseconds(_preTxDelayUs);
  }

  for (size_t i = 0; i < reqSize; ++i) {
    _serial->write(reqArray[i]);
  }
  _serial->flush();

  if (_postTxDelayUs > 0) {
    delayMicroseconds(_postTxDelayUs);
  }

  _dir.setTX(false);

  if (afterReqDelayMs > 0) {
    delay(afterReqDelayMs);
  }

  logPrintln(F("[RS485] Request sent"), debug);
}

size_t RS485Bus::Read_RS485(uint16_t readTimeoutMs, bool debug) {
  if (!_serial) return 0;

  memset(_rxBuf, 0, sizeof(_rxBuf));
  _rxLen = 0;
  _lastFrameOffset = (size_t)-1;

  logPrintln(F("[RS485] Read_RS485() called"), debug);
  logPrint(F("[RS485] Waiting for response, timeout ms = "), debug);
  logPrintDec((unsigned long)readTimeoutMs, debug);
  logNewLine(debug);

  const unsigned long startTime = millis();
  size_t previousLen = 0;

  while ((millis() - startTime) < readTimeoutMs && _rxLen < RX_BUFFER_SIZE) {
    while (_serial->available() > 0 && _rxLen < RX_BUFFER_SIZE) {
      int value = _serial->read();
      if (value >= 0) {
        _rxBuf[_rxLen++] = (uint8_t)value;
      }
    }

    if (_rxLen != previousLen) {
      logPrint(F("[RS485] RX bytes now = "), debug);
      logPrintDec((unsigned long)_rxLen, debug);
      logPrint(F(" at t="), debug);
      logPrintDec((unsigned long)(millis() - startTime), debug);
      logPrintln(F(" ms"), debug);
      previousLen = _rxLen;
    }

    delay(1);
  }

  logPrint(F("[RS485] Read finished, final RX length = "), debug);
  logPrintDec((unsigned long)_rxLen, debug);
  logNewLine(debug);

  if (_rxLen == 0) {
    logPrintln(F("[RS485] RX buffer is empty"), debug);
  } else {
    logBufferMatrix(_rxBuf, _rxLen, "Full RX buffer", debug);
  }

  return _rxLen;
}

bool RS485Bus::Check_Res(const uint8_t resArray[],
                         size_t resArraySize,
                         const uint8_t checkArray[],
                         size_t checkArraySize,
                         bool debug) const {
  if (!resArray || !checkArray) return false;
  if (resArraySize < 3 || checkArraySize == 0 || checkArraySize > resArraySize) {
    return false;
  }

  logPrintln(F("[RS485] Check_Res() called"), debug);

  for (size_t i = 0; i < checkArraySize; ++i) {
    logPrint(F("[RS485] Prefix compare idx "), debug);
    logPrintDec((unsigned long)i, debug);
    logPrint(F(": got 0x"), debug);
    logPrintHexByte(resArray[i], debug);
    logPrint(F(" expected 0x"), debug);
    logPrintHexByte(checkArray[i], debug);
    logNewLine(debug);

    if (resArray[i] != checkArray[i]) {
      logPrintln(F("[RS485] Prefix mismatch -> frame invalid"), debug);
      return false;
    }
  }

  const uint16_t crcCalc = crc16Modbus(resArray, resArraySize - 2);
  const uint16_t crcRes = (uint16_t)resArray[resArraySize - 2] |
                          ((uint16_t)resArray[resArraySize - 1] << 8);

  logPrint(F("[RS485] CRC compare: calc=0x"), debug);
  logPrintHex(crcCalc, debug);
  logPrint(F(" recv=0x"), debug);
  logPrintHex(crcRes, debug);
  logNewLine(debug);

  if (crcCalc == crcRes) {
    logPrintln(F("[RS485] CRC OK -> frame valid"), debug);
    return true;
  }

  logPrintln(F("[RS485] CRC FAIL -> frame invalid"), debug);
  return false;
}

void RS485Bus::ShiftArray(uint8_t array[], size_t arraySize, bool debug) const {
  if (!array || arraySize == 0) return;

  const uint8_t first = array[0];
  for (size_t i = 0; i < arraySize - 1; ++i) {
    array[i] = array[i + 1];
  }
  array[arraySize - 1] = first;

  logBufferMatrix(array, arraySize, "ShiftArray result", debug);
}

bool RS485Bus::SendRequest(uint8_t request[],
                           size_t requestSize,
                           uint8_t getData[],
                           size_t getDataSize,
                           const uint8_t checkCode[],
                           size_t checkCodeSize,
                           uint8_t maxRetries,
                           uint16_t readTimeoutMs,
                           bool debug,
                           uint16_t afterReqDelayMs) {
  if (!_serial || !request || !getData || !checkCode) return false;
  if (requestSize < 2 || getDataSize < 3 || checkCodeSize == 0) return false;
  if (maxRetries == 0) maxRetries = 1;

  memset(getData, 0, getDataSize);

  logSeparator(debug);
  logPrintln(F("[RS485] SendRequest() START"), debug);
  logPrint(F("[RS485] requestSize = "), debug);
  logPrintDec((unsigned long)requestSize, debug);
  logNewLine(debug);

  logPrint(F("[RS485] getDataSize = "), debug);
  logPrintDec((unsigned long)getDataSize, debug);
  logNewLine(debug);

  logPrint(F("[RS485] checkCodeSize = "), debug);
  logPrintDec((unsigned long)checkCodeSize, debug);
  logNewLine(debug);

  logPrint(F("[RS485] maxRetries = "), debug);
  logPrintDec((unsigned long)maxRetries, debug);
  logNewLine(debug);

  logPrint(F("[RS485] readTimeoutMs = "), debug);
  logPrintDec((unsigned long)readTimeoutMs, debug);
  logNewLine(debug);

  logPrint(F("[RS485] afterReqDelayMs = "), debug);
  logPrintDec((unsigned long)afterReqDelayMs, debug);
  logNewLine(debug);

  logBufferMatrix(request, requestSize, "Initial request buffer", debug);
  logBufferMatrix(checkCode, checkCodeSize, "Expected prefix/check code", debug);

  for (uint8_t attempt = 1; attempt <= maxRetries; ++attempt) {
    logSeparator(debug);
    logPrint(F("[RS485] BUS attempt "), debug);
    logPrintDec((unsigned long)attempt, debug);
    logPrint(F(" of "), debug);
    logPrintDec((unsigned long)maxRetries, debug);
    logNewLine(debug);

    CRC_Calc(request, requestSize, debug);
    Request_RS485(request, requestSize, afterReqDelayMs, debug);
    const size_t bytesRead = Read_RS485(readTimeoutMs, debug);

    if (bytesRead == 0) {
      logPrintln(F("[RS485] No response bytes received in this BUS attempt"), debug);
      delay(100UL * attempt);
      continue;
    }

    logTraceRaw(debug);

    if (bytesRead < getDataSize) {
      logPrint(F("[RS485] Buffer shorter than expected frame. bytesRead="), debug);
      logPrintDec((unsigned long)bytesRead, debug);
      logPrint(F(" expected frame len="), debug);
      logPrintDec((unsigned long)getDataSize, debug);
      logNewLine(debug);
      delay(100UL * attempt);
      continue;
    }

    for (size_t offset = 0; offset <= (bytesRead - getDataSize); ++offset) {
      logSeparator(debug);
      logPrint(F("[RS485] Checking frame window at offset "), debug);
      logPrintDec((unsigned long)offset, debug);
      logNewLine(debug);

      logWindow(_rxBuf, offset, getDataSize, debug);

      if (Check_Res(&_rxBuf[offset], getDataSize, checkCode, checkCodeSize, debug)) {
        memcpy(getData, &_rxBuf[offset], getDataSize);
        _lastFrameOffset = offset;

        logPrint(F("[RS485] VALID FRAME FOUND at offset "), debug);
        logPrintDec((unsigned long)offset, debug);
        logNewLine(debug);

        logBufferMatrix(getData, getDataSize, "Copied getData frame", debug);

        logPrintln(F("[RS485] SendRequest() SUCCESS"), debug);
        logSeparator(debug);
        return true;
      } else {
        logPrintln(F("[RS485] This window is not valid"), debug);
      }
    }

    logPrintln(F("[RS485] No valid frame found in RX buffer for this BUS attempt"), debug);
    flushInput();
    delay(100UL * attempt);
  }

  logPrintln(F("[RS485] SendRequest() FAILED after all BUS attempts"), debug);
  logSeparator(debug);
  return false;
}

uint16_t RS485Bus::crc16Modbus(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

bool RS485Bus::verifyCrc16ModbusFrame(const uint8_t *frame, size_t len) {
  if (!frame || len < 3) return false;
  const uint16_t calc = crc16Modbus(frame, len - 2);
  const uint16_t recv = (uint16_t)frame[len - 2] |
                        ((uint16_t)frame[len - 1] << 8);
  return (calc == recv);
}

void RS485Bus::logPrint(const __FlashStringHelper *msg, bool debug) const {
  if (_log) _log->print(msg, debug);
}

void RS485Bus::logPrint(const char *msg, bool debug) const {
  if (_log) _log->print(msg, debug);
}

void RS485Bus::logPrintDec(unsigned long value, bool debug) const {
  if (_log) _log->print(value, debug, "", DEC);
}

void RS485Bus::logPrintHex(unsigned long value, bool debug) const {
  if (_log) _log->print(value, debug, "", HEX);
}

void RS485Bus::logPrintHexByte(uint8_t value, bool debug) const {
  if (_log) _log->print(value, debug, "", HEX);
}

void RS485Bus::logPrintln(const __FlashStringHelper *msg, bool debug) const {
  if (_log) _log->println(msg, debug);
}

void RS485Bus::logPrintln(const char *msg, bool debug) const {
  if (_log) _log->println(msg, debug);
}

void RS485Bus::logNewLine(bool debug) const {
  if (_log) _log->println("", debug);
}

void RS485Bus::logSeparator(bool debug) const {
  if (!_log) return;
  _log->println(F(""), debug);
  _log->println(F("============================================================"), debug);
}

void RS485Bus::logIO(const uint8_t *buf, size_t len, bool debug) const {
  if (!_log || !buf) return;

  for (size_t i = 0; i < len; ++i) {
    _log->print(buf[i], debug, "", HEX);
    if (i + 1 < len) {
      _log->print(F(" "), debug);
    }
  }
  _log->println("", debug);
}

void RS485Bus::logBufferMatrix(const uint8_t *buf, size_t len, const char *title, bool debug) const {
  if (!_log || !buf) return;

  _log->print(F("[RS485] "), debug);
  _log->println(title, debug);

  if (len == 0) {
    _log->println(F("  <empty>"), debug);
    return;
  }

  const size_t columns = (LOG_BUFFER_MATRIX_COLUMNS == 0) ? 8 : LOG_BUFFER_MATRIX_COLUMNS;
  for (size_t rowStart = 0; rowStart < len; rowStart += columns) {
    size_t rowEnd = rowStart + columns;
    if (rowEnd > len) rowEnd = len;

    _log->print(F("  ["), debug);
    _log->print((unsigned long)rowStart, debug, "", DEC);
    _log->print(F("] "), debug);

    for (size_t i = rowStart; i < rowEnd; ++i) {
      _log->print(buf[i], debug, "", HEX);
      if (i + 1 < rowEnd) {
        _log->print(F(" "), debug);
      }
    }
    _log->println("", debug);
  }
}

void RS485Bus::logWindow(const uint8_t *buf, size_t offset, size_t windowLen, bool debug) const {
  if (!_log || !buf) return;
  logBufferMatrix(buf + offset, windowLen, "Current frame window", debug);
}

void RS485Bus::logTraceRaw(bool debug) const {
  if (!_log) return;
  logBufferMatrix(_rxBuf, _rxLen, "Full RX buffer as matrix", debug);
}
