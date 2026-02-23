#pragma once
#include <Arduino.h>
#include <PrintController.h>

/*
  RS485Bus: transport-only library.

  - No sensor logic.
  - No address-change logic.
  - No register knowledge.
  - It only:
    1) sends TX bytes
    2) captures RX raw stream into an internal buffer
    3) optionally extracts a fixed-length frame by matching a prefix
    4) provides Modbus CRC16 helpers

  Direction control:
  - If dePin == -1 => assumes auto-direction RS485 module (no pin toggling)
  - If dePin >= 0  => toggles DE/RE (or DE) pin before/after TX
*/

class RS485Bus {
public:
  // Buffer sizing:
  // - AVR (Mega2560): keep it modest by default (256)
  // - ESP32: can afford bigger (512)
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
    int8_t pin;         // -1 => disabled
    bool activeHighTX;  // true: HIGH=TX, LOW=RX

    DirectionControl(int8_t p = -1, bool activeHigh = true)
    : pin(p), activeHighTX(activeHigh) {}

    void begin() const {
      if (pin >= 0) {
        pinMode(pin, OUTPUT);
        // Default to RX
        digitalWrite(pin, activeHighTX ? LOW : HIGH);
      }
    }

    void setTX(bool tx) const {
      if (pin < 0) return;
      if (activeHighTX) digitalWrite(pin, tx ? HIGH : LOW);
      else              digitalWrite(pin, tx ? LOW  : HIGH);
    }
  };

  RS485Bus();

  // Initialize the underlying serial.
  // For ESP32 you can pass rxPin/txPin; for AVR they are ignored.
  void begin(HardwareSerial& serial,
             uint32_t baud,
             int8_t rxPin = -1,
             int8_t txPin = -1,
             uint32_t config = SERIAL_8N1);

  // Optional DE/RE control (can be set before or after begin()).
  void setDirectionControl(int8_t dePin, bool activeHighTX = true);

  // Logger is optional. If nullptr, no logs.
  void setLogger(PrintController* logger);

  // Transport tuning:
  // preTxDelayUs/postTxDelayUs are small guard times around direction switching.
  void setTimings(uint16_t preTxDelayUs, uint16_t postTxDelayUs);

  // Clears any bytes already waiting in UART RX queue.
  void flushInput();

  // Low-level transfer:
  // - send TX
  // - capture raw RX into internal buffer
  // Returns true if any RX bytes were captured.
  bool transferRaw(const uint8_t* tx, size_t txLen,
                   uint32_t overallTimeoutMs = 250,
                   uint32_t interByteTimeoutMs = 25);

  // Extract a fixed-length frame by matching a prefix inside the captured raw buffer.
  // Example prefix for Modbus: {addr, func, byteCount}
  bool extractFixedFrameByPrefix(const uint8_t* prefix, size_t prefixLen,
                                 size_t expectedFrameLen,
                                 uint8_t* outFrame, size_t outFrameMax,
                                 size_t* outIndex = nullptr) const;

  // Combined convenience:
  // - transferRaw()
  // - extractFixedFrameByPrefix()
  bool transferAndExtractFixedFrame(const uint8_t* tx, size_t txLen,
                                   const uint8_t* prefix, size_t prefixLen,
                                   size_t expectedFrameLen,
                                   uint8_t* outFrame, size_t outFrameMax,
                                   uint32_t overallTimeoutMs = 250,
                                   uint32_t interByteTimeoutMs = 25,
                                   size_t* outIndex = nullptr);

  // Access raw capture.
  const uint8_t* rawData() const;
  size_t rawLength() const;

  // CRC16 Modbus (little-endian in frames: CRC Lo, CRC Hi)
  static uint16_t crc16Modbus(const uint8_t* data, size_t len);
  static bool verifyCrc16ModbusFrame(const uint8_t* frame, size_t len);

private:
  HardwareSerial* _serial;
  DirectionControl _dir;
  PrintController* _log;

  uint16_t _preTxDelayUs;
  uint16_t _postTxDelayUs;

  uint8_t _rxBuf[RX_BUFFER_SIZE];
  size_t _rxLen;

  void logSummaryTransfer(bool gotAny, size_t txLen, size_t rxLen) const;
  void logIOTransfer(const uint8_t* tx, size_t txLen) const;
  void logTraceRaw() const;
};
