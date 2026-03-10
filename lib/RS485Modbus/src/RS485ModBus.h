#pragma once
#include <Arduino.h>
#include <PrintController.h>

/*
  RS485Bus - transport-only helper for RS485 / Modbus RTU.

  What this library does:
    - Starts a HardwareSerial port for RS485 work
    - Optionally toggles a DE/RE direction pin
    - Calculates Modbus CRC16
    - Sends caller-provided request bytes
    - Captures raw incoming bytes into an internal buffer
    - Searches the internal buffer for a caller-defined valid frame
    - Copies the clean valid frame into the caller's GetData[] buffer

  What this library does NOT do:
    - It does not know any sensor registers
    - It does not know what payload bytes mean
    - It does not change addresses on its own
    - It does not apply sensor-specific rules
*/

class RS485Bus {
public:
#if !defined(RS485BUS_RX_BUFFER_SIZE)
  #if defined(ARDUINO_ARCH_AVR)
    static const size_t RX_BUFFER_SIZE = 256;
  #else
    static const size_t RX_BUFFER_SIZE = 512;
  #endif
#else
  static const size_t RX_BUFFER_SIZE = RS485BUS_RX_BUFFER_SIZE;
#endif

  struct DirectionControl {
    int8_t pin;
    bool activeHighTX;

    DirectionControl(int8_t p = -1, bool activeHigh = true)
        : pin(p), activeHighTX(activeHigh) {}

    void begin() const {
      if (pin >= 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, activeHighTX ? LOW : HIGH);
      }
    }

    void setTX(bool tx) const {
      if (pin < 0) return;
      if (activeHighTX) {
        digitalWrite(pin, tx ? HIGH : LOW);
      } else {
        digitalWrite(pin, tx ? LOW : HIGH);
      }
    }
  };

  RS485Bus();

  void begin(HardwareSerial &serial,
             uint32_t baud,
             int8_t rxPin = -1,
             int8_t txPin = -1,
             uint32_t config = SERIAL_8N1);

  void setDirectionControl(int8_t dePin, bool activeHighTX = true);
  void setDebug(PrintController *logger);
  void setTimings(uint16_t preTxDelayUs, uint16_t postTxDelayUs);
  void flushInput();

  void CRC_Calc(uint8_t array[], size_t arraySize, bool debug = false);

  void Request_RS485(const uint8_t reqArray[],
                     size_t reqSize,
                     uint16_t afterReqDelayMs = 10,
                     bool debug = false);

  size_t Read_RS485(uint16_t readTimeoutMs = 2000, bool debug = false);

  bool Check_Res(const uint8_t resArray[],
                 size_t resArraySize,
                 const uint8_t checkArray[],
                 size_t checkArraySize,
                 bool debug = false) const;

  void ShiftArray(uint8_t array[], size_t arraySize, bool debug = false) const;

  bool SendRequest(uint8_t request[],
                   size_t requestSize,
                   uint8_t getData[],
                   size_t getDataSize,
                   const uint8_t checkCode[],
                   size_t checkCodeSize,
                   uint8_t maxRetries = 3,
                   uint16_t readTimeoutMs = 2000,
                   bool debug = false,
                   uint16_t afterReqDelayMs = 10);

  const uint8_t *rawData() const { return _rxBuf; }
  size_t rawLength() const { return _rxLen; }
  size_t lastFrameOffset() const { return _lastFrameOffset; }

  static uint16_t crc16Modbus(const uint8_t *data, size_t len);
  static bool verifyCrc16ModbusFrame(const uint8_t *frame, size_t len);

private:
  HardwareSerial *_serial;
  PrintController *_log;
  DirectionControl _dir;

  uint16_t _preTxDelayUs;
  uint16_t _postTxDelayUs;

  uint8_t _rxBuf[RX_BUFFER_SIZE];
  size_t _rxLen;
  size_t _lastFrameOffset;

  void logPrint(const __FlashStringHelper *msg, bool debug) const;
  void logPrint(const char *msg, bool debug) const;
  void logPrintDec(unsigned long value, bool debug) const;
  void logPrintHex(unsigned long value, bool debug) const;
  void logPrintHexByte(uint8_t value, bool debug) const;
  void logPrintln(const __FlashStringHelper *msg, bool debug) const;
  void logPrintln(const char *msg, bool debug) const;
  void logNewLine(bool debug) const;
  void logIO(const uint8_t *buf, size_t len, bool debug) const;
  void logTraceRaw(bool debug) const;
};