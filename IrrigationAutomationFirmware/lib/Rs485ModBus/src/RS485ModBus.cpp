#include "RS485ModBus.h"

#include <string.h>

RS485Bus::RS485Bus()
    : serial_(nullptr),
      logger_(nullptr),
      directionMode_(Rs485DirectionMode::Automatic),
      directionPin_(-1),
      directionActiveHighTx_(true),
      preTxDelayUs_(0),
      postTxDelayUs_(0),
      interByteTimeoutMs_(5),
      rxBufferConfigured_(false),
      rxLength_(0),
      lastFrameOffset_(static_cast<size_t>(-1)),
      txLength_(0) {
  memset(rxBuffer_, 0, sizeof(rxBuffer_));
  memset(txBuffer_, 0, sizeof(txBuffer_));
}

void RS485Bus::begin(HardwareSerial &serial, uint32_t baud, int8_t rxPin,
                     int8_t txPin, uint32_t config) {
  serial_ = &serial;
#if defined(ARDUINO_ARCH_ESP32)
  if (!rxBufferConfigured_) {
    serial_->setRxBufferSize(RX_BUFFER_SIZE);
    rxBufferConfigured_ = true;
  }
  serial_->begin(baud, config, rxPin, txPin);
#else
  (void)rxPin;
  (void)txPin;
  serial_->begin(baud, config);
#endif
  beginDirection();
  flushInput();
}

void RS485Bus::setDirectionMode(Rs485DirectionMode mode,
                                int8_t directionPin, bool activeHighTx) {
  directionMode_ = mode;
  directionPin_ = directionPin;
  directionActiveHighTx_ = activeHighTx;
  beginDirection();
}

void RS485Bus::setDirectionControl(int8_t directionPin, bool activeHighTx) {
  setDirectionMode(Rs485DirectionMode::Manual, directionPin, activeHighTx);
}

void RS485Bus::beginDirection() {
  if (directionMode_ != Rs485DirectionMode::Manual || directionPin_ < 0)
    return;
  pinMode(directionPin_, OUTPUT);
  setTransmitMode(false);
}

void RS485Bus::setTransmitMode(bool transmit) {
  if (directionMode_ != Rs485DirectionMode::Manual || directionPin_ < 0)
    return;
  const bool high = directionActiveHighTx_ ? transmit : !transmit;
  digitalWrite(directionPin_, high ? HIGH : LOW);
}

void RS485Bus::setDebug(PrintController *logger) { logger_ = logger; }

void RS485Bus::setTimings(uint16_t preTxDelayUs, uint16_t postTxDelayUs,
                          uint16_t interByteTimeoutMs) {
  preTxDelayUs_ = preTxDelayUs;
  postTxDelayUs_ = postTxDelayUs;
  interByteTimeoutMs_ = interByteTimeoutMs == 0 ? 1 : interByteTimeoutMs;
}

void RS485Bus::flushInput() {
  if (serial_ == nullptr) return;
  while (serial_->available() > 0) {
    (void)serial_->read();
    delay(1);
  }
}

void RS485Bus::CRC_Calc(uint8_t array[], size_t arraySize, bool debug) {
  if (array == nullptr || arraySize < 3) return;
  const uint16_t crc = modbus::crc16(array, arraySize - 2);
  array[arraySize - 2] = static_cast<uint8_t>(crc & 0xFFU);
  array[arraySize - 1] = static_cast<uint8_t>((crc >> 8U) & 0xFFU);
  if (logger_ != nullptr && debug) {
    logger_->print(F("[MODBUS] CRC=0x"), true);
    logger_->println(static_cast<unsigned long>(crc), true, "", HEX);
  }
}

void RS485Bus::Request_RS485(const uint8_t request[], size_t requestSize,
                             uint16_t afterRequestDelayMs, bool debug) {
  if (serial_ == nullptr || request == nullptr || requestSize == 0) return;
  txLength_ = requestSize < TX_BUFFER_SIZE ? requestSize : TX_BUFFER_SIZE;
  memcpy(txBuffer_, request, txLength_);
  logFrame(F("[MODBUS] TX: "), request, requestSize, debug);

  flushInput();
  setTransmitMode(true);
  if (preTxDelayUs_ > 0) delayMicroseconds(preTxDelayUs_);
  serial_->write(request, requestSize);
  serial_->flush();
  if (postTxDelayUs_ > 0) delayMicroseconds(postTxDelayUs_);
  setTransmitMode(false);
  if (afterRequestDelayMs > 0) delay(afterRequestDelayMs);
}

size_t RS485Bus::Read_RS485(uint16_t readTimeoutMs, bool debug) {
  if (serial_ == nullptr) return 0;
  memset(rxBuffer_, 0, sizeof(rxBuffer_));
  rxLength_ = 0;
  lastFrameOffset_ = static_cast<size_t>(-1);

  const uint32_t started = millis();
  uint32_t lastByteAt = started;
  bool receivedAny = false;
  while ((millis() - started) < readTimeoutMs && rxLength_ < RX_BUFFER_SIZE) {
    while (serial_->available() > 0 && rxLength_ < RX_BUFFER_SIZE) {
      const int value = serial_->read();
      if (value >= 0) {
        rxBuffer_[rxLength_++] = static_cast<uint8_t>(value);
        lastByteAt = millis();
        receivedAny = true;
      }
    }
    if (receivedAny && (millis() - lastByteAt) >= interByteTimeoutMs_) break;
    delay(1);
  }
  if (rxLength_ > 0) logFrame(F("[MODBUS] RX: "), rxBuffer_, rxLength_, debug);
  return rxLength_;
}

Rs485Result RS485Bus::transact(const uint8_t *requestWithoutCrc,
                               size_t requestLength, uint16_t timeoutMs,
                               bool debug) {
  return rawTransaction(requestWithoutCrc, requestLength, true, timeoutMs,
                        debug);
}

Rs485Result RS485Bus::rawTransaction(const uint8_t *request,
                                     size_t requestLength, bool appendCrc,
                                     uint16_t timeoutMs, bool debug) {
  lastResult_ = {};
  if (serial_ == nullptr) {
    lastResult_.status = Rs485Status::NotInitialized;
    return lastResult_;
  }
  const size_t totalLength = requestLength + (appendCrc ? 2U : 0U);
  if (request == nullptr || requestLength < 2) {
    lastResult_.status = Rs485Status::InvalidArgument;
    return lastResult_;
  }
  if (totalLength > TX_BUFFER_SIZE) {
    lastResult_.status = Rs485Status::RequestTooLarge;
    return lastResult_;
  }

  memcpy(txBuffer_, request, requestLength);
  txLength_ = totalLength;
  if (appendCrc) {
    const uint16_t crc = modbus::crc16(txBuffer_, requestLength);
    txBuffer_[requestLength] = static_cast<uint8_t>(crc & 0xFFU);
    txBuffer_[requestLength + 1] = static_cast<uint8_t>(crc >> 8U);
  } else if (!modbus::verifyFrame(txBuffer_, txLength_)) {
    lastResult_.status = Rs485Status::CrcError;
    logStatus(lastResult_, debug);
    return lastResult_;
  }

  const uint8_t expectedAddress = txBuffer_[0];
  const uint8_t expectedFunction = txBuffer_[1];
  Request_RS485(txBuffer_, txLength_, 0, debug);
  Read_RS485(timeoutMs, debug);
  selectResponseFrame(expectedAddress, expectedFunction);
  lastResult_ = validateResponse(expectedAddress, expectedFunction, debug);
  logStatus(lastResult_, debug);
  return lastResult_;
}

bool RS485Bus::selectResponseFrame(uint8_t expectedAddress,
                                   uint8_t expectedFunction) {
  if (rxLength_ < 5) return false;
  for (size_t offset = 0; offset + 5 <= rxLength_; ++offset) {
    if (expectedAddress != 0 && rxBuffer_[offset] != expectedAddress) continue;
    const uint8_t function = rxBuffer_[offset + 1];
    size_t candidateLength = 0;
    if (function == (expectedFunction | 0x80U)) {
      candidateLength = 5;
    } else if (function != expectedFunction) {
      continue;
    } else if (expectedFunction == 0x03) {
      if (offset + 8 <= rxLength_ && rxBuffer_[offset + 2] == 0xFF &&
          rxBuffer_[offset + 3] == 0x01) {
        candidateLength = 8;
      } else {
        candidateLength = static_cast<size_t>(rxBuffer_[offset + 2]) + 5U;
      }
    } else if (expectedFunction == 0x06) {
      candidateLength = 8;
    }

    if (candidateLength > 0) {
      if (offset + candidateLength > rxLength_) continue;
      if (!modbus::verifyFrame(&rxBuffer_[offset], candidateLength)) continue;
      lastFrameOffset_ = offset;
      if (offset > 0)
        memmove(rxBuffer_, &rxBuffer_[offset], candidateLength);
      rxLength_ = candidateLength;
      return true;
    }

    // Generic raw diagnostic fallback for other Modbus functions.
    for (size_t length = 5; offset + length <= rxLength_; ++length) {
      if (!modbus::verifyFrame(&rxBuffer_[offset], length)) continue;
      lastFrameOffset_ = offset;
      if (offset > 0) memmove(rxBuffer_, &rxBuffer_[offset], length);
      rxLength_ = length;
      return true;
    }
  }
  return false;
}

Rs485Result RS485Bus::validateResponse(uint8_t expectedAddress,
                                       uint8_t expectedFunction,
                                       bool debug) {
  (void)debug;
  Rs485Result result;
  result.responseLength = rxLength_;
  if (rxLength_ == 0) {
    result.status = Rs485Status::Timeout;
    return result;
  }
  if (!modbus::verifyFrame(rxBuffer_, rxLength_)) {
    result.status = Rs485Status::CrcError;
    return result;
  }
  if (expectedAddress != 0 && rxBuffer_[0] != expectedAddress) {
    result.status = Rs485Status::AddressMismatch;
    return result;
  }
  if (rxLength_ >= 5 && rxBuffer_[1] == (expectedFunction | 0x80U)) {
    result.status = Rs485Status::ModbusException;
    result.exceptionCode = rxBuffer_[2];
    return result;
  }
  if (rxBuffer_[1] != expectedFunction) {
    result.status = Rs485Status::FunctionMismatch;
    return result;
  }
  // CDI-E uses FF01 + a 16-bit error code in an otherwise normal response.
  if (rxLength_ >= 8 && rxBuffer_[2] == 0xFF && rxBuffer_[3] == 0x01) {
    result.status = Rs485Status::DeviceError;
    result.deviceErrorCode =
        (static_cast<uint16_t>(rxBuffer_[4]) << 8U) | rxBuffer_[5];
    return result;
  }
  result.status = Rs485Status::Ok;
  return result;
}

bool RS485Bus::Check_Res(const uint8_t response[], size_t responseSize,
                         const uint8_t prefix[], size_t prefixSize,
                         bool debug) const {
  if (response == nullptr || prefix == nullptr || responseSize < 3 ||
      prefixSize > responseSize)
    return false;
  for (size_t i = 0; i < prefixSize; ++i) {
    if (response[i] != prefix[i]) return false;
  }
  const bool valid = modbus::verifyFrame(response, responseSize);
  if (logger_ != nullptr && debug)
    logger_->println(valid ? F("[MODBUS] CRC OK") : F("[MODBUS] CRC FAIL"),
                     true);
  return valid;
}

void RS485Bus::ShiftArray(uint8_t array[], size_t arraySize, bool debug) const {
  if (array == nullptr || arraySize == 0) return;
  const uint8_t first = array[0];
  for (size_t i = 0; i + 1 < arraySize; ++i) array[i] = array[i + 1];
  array[arraySize - 1] = first;
  logFrame(F("[MODBUS] Shifted: "), array, arraySize, debug);
}

bool RS485Bus::SendRequest(uint8_t request[], size_t requestSize,
                           uint8_t response[], size_t responseSize,
                           const uint8_t prefix[], size_t prefixSize,
                           uint8_t maxRetries, uint16_t readTimeoutMs,
                           bool debug, uint16_t afterRequestDelayMs) {
  if (request == nullptr || response == nullptr || prefix == nullptr ||
      requestSize < 3 || responseSize < 3 || maxRetries == 0)
    return false;
  memset(response, 0, responseSize);
  for (uint8_t attempt = 0; attempt < maxRetries; ++attempt) {
    CRC_Calc(request, requestSize, debug);
    Request_RS485(request, requestSize, afterRequestDelayMs, debug);
    const size_t received = Read_RS485(readTimeoutMs, debug);
    if (received >= responseSize) {
      for (size_t offset = 0; offset <= received - responseSize; ++offset) {
        if (Check_Res(&rxBuffer_[offset], responseSize, prefix, prefixSize,
                      debug)) {
          memcpy(response, &rxBuffer_[offset], responseSize);
          lastFrameOffset_ = offset;
          return true;
        }
      }
    }
    flushInput();
    delay(50U * (attempt + 1U));
  }
  return false;
}

void RS485Bus::logFrame(const __FlashStringHelper *prefix,
                        const uint8_t *data, size_t length, bool debug) const {
  if (logger_ == nullptr || !debug || data == nullptr) return;
  logger_->print(prefix, true);
  for (size_t i = 0; i < length; ++i) {
    logger_->print(data[i], true, "", HEX);
    if (i + 1 < length) logger_->print(' ', true);
  }
  logger_->println(true);
}

void RS485Bus::logStatus(const Rs485Result &result, bool debug) const {
  if (logger_ == nullptr || !debug) return;
  logger_->print(F("[MODBUS] Result: "), true);
  logger_->print(statusName(result.status), true);
  logger_->print(F(", address="), true);
  logger_->print(txLength_ > 0 ? txBuffer_[0] : 0, true, "", DEC);
  logger_->print(F(", bytes="), true);
  logger_->println(static_cast<unsigned long>(result.responseLength), true);
  if (result.status == Rs485Status::ModbusException) {
    logger_->print(F("[MODBUS] Exception=0x"), true);
    logger_->println(result.exceptionCode, true, "", HEX);
  } else if (result.status == Rs485Status::DeviceError) {
    logger_->print(F("[MODBUS] CDI-E error="), true);
    logger_->println(static_cast<unsigned long>(result.deviceErrorCode), true);
  }
}

const char *RS485Bus::statusName(Rs485Status status) {
  switch (status) {
    case Rs485Status::Ok:
      return "OK";
    case Rs485Status::NotInitialized:
      return "not initialized";
    case Rs485Status::InvalidArgument:
      return "invalid argument";
    case Rs485Status::RequestTooLarge:
      return "request too large";
    case Rs485Status::Timeout:
      return "timeout";
    case Rs485Status::CrcError:
      return "CRC error";
    case Rs485Status::AddressMismatch:
      return "address mismatch";
    case Rs485Status::FunctionMismatch:
      return "function mismatch";
    case Rs485Status::ModbusException:
      return "Modbus exception";
    case Rs485Status::DeviceError:
      return "CDI-E device error";
  }
  return "unknown";
}
