#pragma once
#include <Arduino.h>
#include "../../../config/Configuration_System.h"
#include "../../../config/Configuration_PCB.h"
#include "../../../config/Configuration_ModbusAddresses.h"

/*
  JXBS Gas Register Scanner Example - local diagnostic config

  This tool reads one Modbus address and one inclusive register range. It sends
  one read request for the whole range, then prints the returned register array.
*/

// Target Modbus slave address.
#define SCANNER_ADDRESS              0x61

// 0x03 = holding registers. 0x04 = input registers.
#define SCANNER_FUNCTION_CODE        0x03

// Inclusive register range.
#define SCANNER_REGISTER_START       0x0000
#define SCANNER_REGISTER_END         0x0016

// Modbus 0x03/0x04 limit: 125 registers per request.
#define SCANNER_MAX_READ_REGISTERS   125

#define SCANNER_RETRIES              2
#define SCANNER_READ_TIMEOUT_MS      1000
#define SCANNER_AFTER_REQ_MS         SENSOR_DEFAULT_AFTER_REQ_MS

// true prints RS485 TX/RX buffers for the request.
#define SCANNER_DEBUG_RAW            true

// Repeat the same single-address scan in loop(). Set false for one scan after boot.
#define SCANNER_REPEAT_SCAN          true
#define SCANNER_POLL_INTERVAL_MS     3000

#define SCANNER_REGISTER_COUNT       (SCANNER_REGISTER_END - SCANNER_REGISTER_START + 1)

#if SCANNER_ADDRESS < 1 || SCANNER_ADDRESS > 247
#error "SCANNER_ADDRESS must be 1..247"
#endif

#if SCANNER_FUNCTION_CODE != 0x03 && SCANNER_FUNCTION_CODE != 0x04
#error "SCANNER_FUNCTION_CODE must be 0x03 or 0x04"
#endif

#if SCANNER_REGISTER_END < SCANNER_REGISTER_START
#error "SCANNER_REGISTER_END must be >= SCANNER_REGISTER_START"
#endif

#if SCANNER_REGISTER_COUNT < 1
#error "SCANNER_REGISTER_COUNT must be at least 1"
#endif

#if SCANNER_REGISTER_COUNT > SCANNER_MAX_READ_REGISTERS
#error "Register range is too large for one Modbus request; max is 125 registers"
#endif
