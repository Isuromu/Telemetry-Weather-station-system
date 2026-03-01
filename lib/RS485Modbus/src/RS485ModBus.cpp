#include "RS485Modbus.h"

RS485Bus::RS485Bus()
    : _serial(nullptr), _dir(-1, true), _log(nullptr), _preTxDelayUs(200),
      _postTxDelayUs(200), _rxBuf{0}, _rxLen(0) {}

void RS485Bus::begin(HardwareSerial &serial, uint32_t baud, int8_t rxPin,
                     int8_t txPin, uint32_t config) {
  _serial = &serial;

// ESP32 allows assigning UART pins at runtime
#if defined(ARDUINO_ARCH_ESP32)
  // Make UART RX buffer bigger to reduce overruns
  // (ESP32 HardwareSerial supports this)
  serial.setRxBufferSize((int)(RX_BUFFER_SIZE + 64));
  serial.begin(baud, config, rxPin, txPin);
#else
  (void)rxPin;
  (void)txPin;
  (void)config;
  serial.begin(baud);
#endif

  _dir.begin();
  flushInput();
}

void RS485Bus::setDirectionControl(int8_t dePin, bool activeHighTX) {
  _dir = DirectionControl(dePin, activeHighTX);
  _dir.begin();
}

void RS485Bus::setLogger(PrintController *logger) { _log = logger; }

void RS485Bus::setTimings(uint16_t preTxDelayUs, uint16_t postTxDelayUs) {
  _preTxDelayUs = preTxDelayUs;
  _postTxDelayUs = postTxDelayUs;
}

void RS485Bus::flushInput() {
  if (!_serial)
    return;
  while (_serial->available() > 0) {
    (void)_serial->read();
    delay(1);
  }
}

bool RS485Bus::transferRaw(const uint8_t *tx, size_t txLen,
                           uint32_t overallTimeoutMs,
                           uint32_t interByteTimeoutMs) {
  if (!_serial || !tx || txLen == 0) {
    _rxLen = 0;
    return false;
  }

  // Clear previous capture
  _rxLen = 0;
  memset(_rxBuf, 0, RX_BUFFER_SIZE);

  // Flush old UART bytes (important if garbage stays from previous cycles)
  flushInput();

  // --- TX ---
  if (_log && _log->isEnabled() && _log->verbosity() >= PrintController::IO) {
    logIOTransfer(tx, txLen);
  }

  if (_log && _log->isEnabled() &&
      _log->verbosity() >= PrintController::TRACE) {
    _log->log(PrintController::TRACE, F("[RS485] Mode -> TX | Pre-TX Delay: "));
    _log->print(_preTxDelayUs);
    _log->println(F("us"));
  }

  _dir.setTX(true);
  if (_preTxDelayUs)
    delayMicroseconds(_preTxDelayUs);

  for (size_t i = 0; i < txLen; i++) {
    _serial->write(tx[i]);
  }
  _serial->flush();

  if (_log && _log->isEnabled() &&
      _log->verbosity() >= PrintController::TRACE) {
    _log->log(PrintController::TRACE,
              F("[RS485] TX flushed | Post-TX Delay: "));
    _log->print(_postTxDelayUs);
    _log->println(F("us"));
  }

  if (_postTxDelayUs)
    delayMicroseconds(_postTxDelayUs);

  if (_log && _log->isEnabled() &&
      _log->verbosity() >= PrintController::TRACE) {
    _log->logln(PrintController::TRACE, F("[RS485] Mode -> RX"));
  }
  _dir.setTX(false);

  // --- RX capture ---
  uint32_t tStart = millis();
  uint32_t tLastByte = 0;
  bool started = false;

  while ((millis() - tStart) < overallTimeoutMs && _rxLen < RX_BUFFER_SIZE) {
    int avail = _serial->available();
    if (avail > 0) {
      if (!started && _log && _log->isEnabled() &&
          _log->verbosity() >= PrintController::TRACE) {
        _log->log(PrintController::TRACE,
                  F("[RS485] RX Capture started after "));
        _log->print(millis() - tStart);
        _log->println(F("ms"));
      }
      started = true;
      while (avail-- > 0 && _rxLen < RX_BUFFER_SIZE) {
        int c = _serial->read();
        if (c < 0)
          break;
        _rxBuf[_rxLen++] = (uint8_t)c;
      }
      tLastByte = millis();
    } else {
      // Stop early if frame already started and bus went silent
      if (started && (millis() - tLastByte) >= interByteTimeoutMs) {
        if (_log && _log->isEnabled() &&
            _log->verbosity() >= PrintController::TRACE) {
          _log->log(
              PrintController::TRACE,
              F("[RS485] RX Capture boundary reached (inter-byte silence: "));
          _log->print(millis() - tLastByte);
          _log->println(F("ms)"));
        }
        break;
      }
      delay(1);
    }
  }

  if (!started && _log && _log->isEnabled() &&
      _log->verbosity() >= PrintController::TRACE) {
    _log->log(PrintController::TRACE, F("[RS485] RX Timeout ("));
    _log->print(overallTimeoutMs);
    _log->println(F("ms reached)"));
  }

  bool gotAny = (_rxLen > 0);

  if (_log && _log->isEnabled()) {
    logSummaryTransfer(gotAny, txLen, _rxLen);
    if (_log->verbosity() >= PrintController::TRACE) {
      logTraceRaw();
    }
  }

  return gotAny;
}

bool RS485Bus::extractFixedFrameByPrefix(const uint8_t *prefix,
                                         size_t prefixLen,
                                         size_t expectedFrameLen,
                                         uint8_t *outFrame, size_t outFrameMax,
                                         size_t *outIndex) const {
  if (!prefix || prefixLen == 0 || expectedFrameLen == 0)
    return false;
  if (!outFrame || outFrameMax < expectedFrameLen)
    return false;
  if (_rxLen < expectedFrameLen || expectedFrameLen < prefixLen)
    return false;

  // Scan raw buffer for prefix match
  for (size_t i = 0; i + expectedFrameLen <= _rxLen; i++) {
    bool ok = true;
    for (size_t k = 0; k < prefixLen; k++) {
      if (_rxBuf[i + k] != prefix[k]) {
        ok = false;
        break;
      }
    }
    if (ok) {
      if (!verifyCrc16ModbusFrame(_rxBuf + i, expectedFrameLen)) {
        if (_log && _log->isEnabled() &&
            _log->verbosity() >= PrintController::TRACE) {
          uint16_t calc = crc16Modbus(_rxBuf + i, expectedFrameLen - 2);
          uint16_t got = (uint16_t)_rxBuf[i + expectedFrameLen - 2] |
                         ((uint16_t)_rxBuf[i + expectedFrameLen - 1] << 8);
          _log->print(F("[RS485] Found prefix at idx="));
          _log->print((unsigned long)i);
          _log->print(F(", but CRC fail: Calc=0x"));
          _log->printHexU16(calc);
          _log->print(F(" Got=0x"));
          _log->printHexU16(got);
          _log->println();
        }
        continue; // Keep searching for another valid frame
      }

      memcpy(outFrame, _rxBuf + i, expectedFrameLen);
      if (outIndex)
        *outIndex = i;
      return true;
    }
  }
  return false;
}

bool RS485Bus::transferAndExtractFixedFrame(
    const uint8_t *tx, size_t txLen, const uint8_t *prefix, size_t prefixLen,
    size_t expectedFrameLen, uint8_t *outFrame, size_t outFrameMax,
    uint32_t overallTimeoutMs, uint32_t interByteTimeoutMs, size_t *outIndex) {
  bool gotAny = transferRaw(tx, txLen, overallTimeoutMs, interByteTimeoutMs);
  if (!gotAny) {
    if (_log && _log->isEnabled() &&
        _log->verbosity() >= PrintController::SUMMARY) {
      _log->logln(PrintController::SUMMARY, F("[RS485] RX: no bytes captured"));
    }
    return false;
  }

  size_t idx = 0;
  bool ok = extractFixedFrameByPrefix(prefix, prefixLen, expectedFrameLen,
                                      outFrame, outFrameMax, &idx);

  if (_log && _log->isEnabled()) {
    if (_log->verbosity() >= PrintController::SUMMARY) {
      _log->print(F("[RS485] Frame "));
      _log->print(ok ? F("FOUND") : F("NOT FOUND"));
      _log->print(F(" (expectedLen="));
      _log->print((unsigned long)expectedFrameLen);
      _log->print(F(", idx="));
      _log->print((unsigned long)(ok ? idx : 0xFFFFFFFFUL));
      _log->println(F(")"));
    }

    if (ok && _log->verbosity() >= PrintController::IO) {
      _log->log(PrintController::IO, F("[RS485] Extracted frame: "));
      _log->dumpHexInline(outFrame, expectedFrameLen);
      _log->println();
    }

    if (_log->verbosity() >= PrintController::TRACE) {
      if (ok) {
        _log->print(F("[RS485] Extract index: "));
        _log->print((unsigned long)idx);
        uint16_t calc = crc16Modbus(outFrame, expectedFrameLen - 2);
        uint16_t got = (uint16_t)outFrame[expectedFrameLen - 2] |
                       ((uint16_t)outFrame[expectedFrameLen - 1] << 8);
        _log->print(F(" | CRC OK (Calc=0x"));
        _log->printHexU16(calc);
        _log->print(F(" Got=0x"));
        _log->printHexU16(got);
        _log->println(F(")"));
      }
    }
  }

  if (outIndex)
    *outIndex = idx;
  return ok;
}

const uint8_t *RS485Bus::rawData() const { return _rxBuf; }
size_t RS485Bus::rawLength() const { return _rxLen; }

// ---- CRC16 Modbus ----
uint16_t RS485Bus::crc16Modbus(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x0001)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc = (crc >> 1);
    }
  }
  return crc;
}

bool RS485Bus::verifyCrc16ModbusFrame(const uint8_t *frame, size_t len) {
  if (!frame || len < 3)
    return false;
  uint16_t calc = crc16Modbus(frame, len - 2);
  uint16_t got = (uint16_t)frame[len - 2] | ((uint16_t)frame[len - 1] << 8);
  return (calc == got);
}

// ---- logging internals ----
void RS485Bus::logSummaryTransfer(bool gotAny, size_t txLen,
                                  size_t rxLen) const {
  if (!_log || !_log->isEnabled() ||
      _log->verbosity() < PrintController::SUMMARY)
    return;

  _log->print(F("[RS485] TX="));
  _log->print((unsigned long)txLen);
  _log->print(F(" bytes, RAW_RX="));
  _log->print((unsigned long)rxLen);
  _log->print(F(" bytes, status="));
  _log->println(gotAny ? F("OK") : F("FAIL"));
}

void RS485Bus::logIOTransfer(const uint8_t *tx, size_t txLen) const {
  if (!_log)
    return;
  _log->log(PrintController::IO, F("[RS485] TX: "));
  _log->dumpHexInline(tx, txLen);
  _log->println();
}

void RS485Bus::logTraceRaw() const {
  if (!_log)
    return;
  _log->log(PrintController::TRACE, F("[RS485] RAW RX used/capacity: "));
  _log->print((unsigned long)_rxLen);
  _log->print(F("/"));
  _log->println((unsigned long)RX_BUFFER_SIZE);

  // In TRACE mode print the whole capture array so unused slots are visible
  // as 00 and index positions can be inspected directly.
  _log->logln(PrintController::TRACE, F("[RS485] RAW RX full buffer (index: 16-byte row)"));
  for (size_t i = 0; i < RX_BUFFER_SIZE; i += 16) {
    _log->print(F("["));
    if (i < 1000) {
      _log->print(F("0"));
    }
    if (i < 100) {
      _log->print(F("0"));
    }
    if (i < 10) {
      _log->print(F("0"));
    }
    _log->print((unsigned long)i);
    _log->print(F("] "));

    size_t rowEnd = i + 16;
    if (rowEnd > RX_BUFFER_SIZE) {
      rowEnd = RX_BUFFER_SIZE;
    }
    for (size_t j = i; j < rowEnd; ++j) {
      _log->printHexByte(_rxBuf[j]);
      if (j + 1 < rowEnd) {
        _log->print(F(" "));
      }
    }
    _log->println();
  }
}
