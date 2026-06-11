#include <Arduino.h>
#include <stdio.h>
#include "config.h"
#include "RS485SensorExampleRuntime.h"

static bool scanAlreadyRan = false;

static void formatHex2(uint8_t value, char* out, size_t outSize) {
  snprintf(out, outSize, "0x%02X", (unsigned int)value);
}

static void formatHex4(uint16_t value, char* out, size_t outSize) {
  snprintf(out, outSize, "0x%04X", (unsigned int)value);
}

static void printExceptionMeaning(uint8_t exceptionCode) {
  printer.print(F(" meaning="), true);
  switch (exceptionCode) {
    case 0x01:
      printer.print(F("illegal function"), true);
      break;
    case 0x02:
      printer.print(F("illegal data address"), true);
      break;
    case 0x03:
      printer.print(F("illegal data value"), true);
      break;
    case 0x04:
      printer.print(F("slave device failure"), true);
      break;
    default:
      printer.print(F("unknown"), true);
      break;
  }
}

static bool findModbusException(uint8_t address, uint8_t functionCode, uint8_t* exceptionCode) {
  const uint8_t* raw = rs485.rawData();
  const size_t rawLen = rs485.rawLength();
  if (!raw || rawLen < 5) {
    return false;
  }

  const uint8_t exceptionFunction = functionCode | 0x80;
  for (size_t offset = 0; offset <= rawLen - 5; ++offset) {
    const uint8_t* frame = &raw[offset];
    if (frame[0] == address &&
        frame[1] == exceptionFunction &&
        RS485Bus::verifyCrc16ModbusFrame(frame, 5)) {
      if (exceptionCode) {
        *exceptionCode = frame[2];
      }
      return true;
    }
  }

  return false;
}

static void printRegisterLine(uint16_t reg, uint16_t word) {
  char regBuf[7];
  char wordBuf[7];
  formatHex4(reg, regBuf, sizeof(regBuf));
  formatHex4(word, wordBuf, sizeof(wordBuf));

  printer.print(F("[REG] reg="), true);
  printer.print(regBuf, true);
  printer.print(F(" raw="), true);
  printer.print(wordBuf, true);
  printer.print(F(" u16="), true);
  printer.print((unsigned int)word, true);
  printer.print(F(" s16="), true);
  printer.print((int)(int16_t)word, true);
  printer.print(F(" /10="), true);
  printer.print((double)word / 10.0, true, "", 3);
  printer.print(F(" /100="), true);
  printer.print((double)word / 100.0, true, "", 3);
  printer.print(F(" /1000="), true);
  printer.print((double)word / 1000.0, true, "", 3);
  printer.println("", true);
}

static bool readConfiguredRegisterRange() {
  static const uint8_t registerCount = (uint8_t)SCANNER_REGISTER_COUNT;
  static const uint8_t responseSize = 5 + (2 * registerCount);

  uint8_t request[8] = {
    SCANNER_ADDRESS,
    SCANNER_FUNCTION_CODE,
    (uint8_t)(SCANNER_REGISTER_START >> 8),
    (uint8_t)(SCANNER_REGISTER_START & 0xFF),
    (uint8_t)(registerCount >> 8),
    (uint8_t)(registerCount & 0xFF),
    0x00,
    0x00
  };

  uint8_t response[responseSize] = {0};
  const uint8_t check[3] = {
    SCANNER_ADDRESS,
    SCANNER_FUNCTION_CODE,
    (uint8_t)(2 * registerCount)
  };

  char addrBuf[7];
  char startBuf[7];
  char endBuf[7];
  formatHex2(SCANNER_ADDRESS, addrBuf, sizeof(addrBuf));
  formatHex4(SCANNER_REGISTER_START, startBuf, sizeof(startBuf));
  formatHex4(SCANNER_REGISTER_END, endBuf, sizeof(endBuf));

  printer.print(F("[SCAN] addr="), true);
  printer.print(addrBuf, true);
  printer.print(F(" function=0x"), true);
  printer.print((unsigned int)SCANNER_FUNCTION_CODE, true, "", HEX);
  printer.print(F(" read registers "), true);
  printer.print(startBuf, true);
  printer.print(F(".."), true);
  printer.print(endBuf, true);
  printer.print(F(" count="), true);
  printer.println((unsigned int)registerCount, true);

  const bool ok = rs485.SendRequest(request,
                                    sizeof(request),
                                    response,
                                    sizeof(response),
                                    check,
                                    sizeof(check),
                                    SCANNER_RETRIES,
                                    SCANNER_READ_TIMEOUT_MS,
                                    SCANNER_DEBUG_RAW,
                                    SCANNER_AFTER_REQ_MS);

  if (!ok) {
    uint8_t exceptionCode = 0;
    printer.print(F("[SCAN] FAIL addr="), true);
    printer.print(addrBuf, true);
    if (findModbusException(SCANNER_ADDRESS, SCANNER_FUNCTION_CODE, &exceptionCode)) {
      char codeBuf[7];
      formatHex2(exceptionCode, codeBuf, sizeof(codeBuf));
      printer.print(F(" exception="), true);
      printer.print(codeBuf, true);
      printExceptionMeaning(exceptionCode);
    } else {
      printer.print(F(" no valid Modbus frame"), true);
    }
    printer.println("", true);
    return false;
  }

  printer.print(F("[SCAN] OK addr="), true);
  printer.print(addrBuf, true);
  printer.print(F(" returned_words="), true);
  printer.println((unsigned int)registerCount, true);

  for (uint8_t i = 0; i < registerCount; ++i) {
    const uint16_t reg = SCANNER_REGISTER_START + i;
    const uint16_t word = ((uint16_t)response[3 + (2 * i)] << 8) |
                          (uint16_t)response[4 + (2 * i)];
    printRegisterLine(reg, word);
  }

  return true;
}

static void runScan() {
  printer.println(F(""), true);
  printer.println(F("============================================================"), true);
  printer.println(F(" JXBS/JXCT Single-Address Register Reader"), true);
  printer.println(F("============================================================"), true);
  printer.println(F("- Edit examples/JXBS_GasRegisterScanner_Example/src/config.h"), true);
  printer.println(F("- One address, one register range, one Modbus read request"), true);

  readConfiguredRegisterRange();

  printer.println(F("[SCAN] complete"), true);
}

void setup() {
  beginRS485SensorExample();
}

void loop() {
  if (!SCANNER_REPEAT_SCAN && scanAlreadyRan) {
    delay(1000);
    return;
  }

  scanAlreadyRan = true;
  runScan();
  delay(SCANNER_POLL_INTERVAL_MS);
}
