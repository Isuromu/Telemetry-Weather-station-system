#pragma once

#include <Arduino.h>
#include <PrintController.h>

#include "ModbusCrc.h"

enum class Rs485DirectionMode : uint8_t {
  Manual,
  Automatic
};

enum class Rs485Status : uint8_t {
  Ok,
  NotInitialized,
  InvalidArgument,
  RequestTooLarge,
  Timeout,
  CrcError,
  AddressMismatch,
  FunctionMismatch,
  ModbusException,
  DeviceError
};

struct Rs485Result {
  Rs485Status status{Rs485Status::NotInitialized};
  size_t responseLength{0};
  uint8_t exceptionCode{0};
  uint16_t deviceErrorCode{0};

  bool ok() const { return status == Rs485Status::Ok; }
};

class RS485Bus {
 public:
#if defined(RS485BUS_RX_BUFFER_SIZE)
  static constexpr size_t RX_BUFFER_SIZE = RS485BUS_RX_BUFFER_SIZE;
#elif defined(ARDUINO_ARCH_AVR)
  static constexpr size_t RX_BUFFER_SIZE = 256;
#else
  static constexpr size_t RX_BUFFER_SIZE = 512;
#endif
  static constexpr size_t TX_BUFFER_SIZE = 64;

  RS485Bus();

  void begin(HardwareSerial &serial, uint32_t baud, int8_t rxPin = -1,
             int8_t txPin = -1, uint32_t config = SERIAL_8N1);
  bool initialized() const { return serial_ != nullptr; }

  void setDirectionMode(Rs485DirectionMode mode, int8_t directionPin = -1,
                        bool activeHighTx = true);
  void setDirectionControl(int8_t directionPin,
                           bool activeHighTx = true);
  Rs485DirectionMode directionMode() const { return directionMode_; }

  void setDebug(PrintController *logger);
  PrintController *getLogger() const { return logger_; }
  void setTimings(uint16_t preTxDelayUs, uint16_t postTxDelayUs,
                  uint16_t interByteTimeoutMs = 5);
  void flushInput();

  Rs485Result transact(const uint8_t *requestWithoutCrc,
                       size_t requestLength, uint16_t timeoutMs = 300,
                       bool debug = false);
  Rs485Result rawTransaction(const uint8_t *request, size_t requestLength,
                             bool appendCrc, uint16_t timeoutMs = 300,
                             bool debug = false);

  // Compatibility API retained from the supplied transport library.
  void CRC_Calc(uint8_t array[], size_t arraySize, bool debug = false);
  void Request_RS485(const uint8_t request[], size_t requestSize,
                     uint16_t afterRequestDelayMs = 0, bool debug = false);
  size_t Read_RS485(uint16_t readTimeoutMs = 2000, bool debug = false);
  bool Check_Res(const uint8_t response[], size_t responseSize,
                 const uint8_t prefix[], size_t prefixSize,
                 bool debug = false) const;
  void ShiftArray(uint8_t array[], size_t arraySize,
                  bool debug = false) const;
  bool SendRequest(uint8_t request[], size_t requestSize, uint8_t response[],
                   size_t responseSize, const uint8_t prefix[],
                   size_t prefixSize, uint8_t maxRetries = 3,
                   uint16_t readTimeoutMs = 2000, bool debug = false,
                   uint16_t afterRequestDelayMs = 0);

  const uint8_t *rawData() const { return rxBuffer_; }
  size_t rawLength() const { return rxLength_; }
  const uint8_t *lastRequest() const { return txBuffer_; }
  size_t lastRequestLength() const { return txLength_; }
  size_t lastFrameOffset() const { return lastFrameOffset_; }
  Rs485Result lastResult() const { return lastResult_; }

  static uint16_t crc16Modbus(const uint8_t *data, size_t length) {
    return modbus::crc16(data, length);
  }
  static bool verifyCrc16ModbusFrame(const uint8_t *frame, size_t length) {
    return modbus::verifyFrame(frame, length);
  }
  static const char *statusName(Rs485Status status);

 private:
  HardwareSerial *serial_;
  PrintController *logger_;
  Rs485DirectionMode directionMode_;
  int8_t directionPin_;
  bool directionActiveHighTx_;
  uint16_t preTxDelayUs_;
  uint16_t postTxDelayUs_;
  uint16_t interByteTimeoutMs_;
  bool rxBufferConfigured_;

  uint8_t rxBuffer_[RX_BUFFER_SIZE];
  size_t rxLength_;
  size_t lastFrameOffset_;
  uint8_t txBuffer_[TX_BUFFER_SIZE];
  size_t txLength_;
  Rs485Result lastResult_;

  void beginDirection();
  void setTransmitMode(bool transmit);
  bool selectResponseFrame(uint8_t expectedAddress,
                           uint8_t expectedFunction);
  Rs485Result validateResponse(uint8_t expectedAddress,
                               uint8_t expectedFunction, bool debug);
  void logFrame(const __FlashStringHelper *prefix, const uint8_t *data,
                size_t length, bool debug) const;
  void logStatus(const Rs485Result &result, bool debug) const;
};
