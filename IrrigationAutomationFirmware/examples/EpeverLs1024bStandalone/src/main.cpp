#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

/*
  EPEVER LS1024B, one 12 V / 9 Ah VRLA battery, 25 W PV.
  Standalone ESP32-WROOM / auto-direction USB-TTL-to-RS485 transceiver example.
  Current Modbus ID: 0x60 (96), serial: 115200 8N1.

  ONE-TIME CONFIGURATION (on boot, only if readback differs):
    Charging limit: 14.40 V; boost/equalize: 14.40 V; float: 13.70 V.
    LOAD low-voltage disconnect: 11.80 V; recovery: 12.50 V.
  LOAD output is battery-tracking, NOT a regulated 14.4 V output.
  Use a correctly rated hardware DC-DC converter when the load's absolute
  input-voltage ceiling is 14.4 V. This code cannot implement that clamp.

  WARNING: controller settings are changed on startup when the switch below
  is enabled. Only use with one controller connected, an already verified
  USER battery type, and a confirmed 12 V VRLA battery. If communication or
  verification fails, existing settings remain uncertain. Do not rely on
  ESP32 polling as the only battery protection.
*/

#ifndef RS485_RX_PIN
#define RS485_RX_PIN 16
#endif
#ifndef RS485_TX_PIN
#define RS485_TX_PIN 17
#endif

#define SENSOR_ID                         "battery"
#define SENSOR_ADDRESS                    0x60
#define SENSOR_RS485_BAUD                 115200UL
#define POLL_INTERVAL_MS                  3000UL
#define MODBUS_TIMEOUT_MS                 400UL
#define SENSOR_FIELD_BATT                 "Batt"
#define SENSOR_FIELD_BATT_TEMP            "BattTemp"

// Set true for a deliberate configuration run; set false after verification.
// NOTE: Writing a complete 0x9003..0x900E block is required by this profile.
#ifndef APPLY_BATTERY_PROFILE_ON_BOOT
#define APPLY_BATTERY_PROFILE_ON_BOOT     false
#endif

// Software setpoints: they affect hardware only if the write succeeds.
#define PROFILE_OVER_VOLTAGE_DISCONNECT_V 14.80f
#define PROFILE_CHARGING_LIMIT_V          14.40f
#define PROFILE_OVER_VOLTAGE_RECONNECT_V  14.60f
#define PROFILE_EQUALIZATION_V            14.40f
#define PROFILE_BOOST_V                   14.40f
#define PROFILE_FLOAT_V                   13.70f
#define PROFILE_BOOST_RECONNECT_V         13.20f
#define PROFILE_LOW_VOLTAGE_RECONNECT_V   12.50f
#define PROFILE_UNDER_VOLTAGE_RECOVER_V   12.20f
#define PROFILE_UNDER_VOLTAGE_WARNING_V   12.00f
#define PROFILE_LOW_VOLTAGE_DISCONNECT_V  11.80f
#define PROFILE_DISCHARGING_LIMIT_V       10.60f

// EPEVER input registers; FC04, /100; signed temperature.
#define EPEVER_REG_BATTERY_VOLTAGE         0x3104
#define EPEVER_REG_BATTERY_TEMPERATURE     0x3110
#define EPEVER_REG_LOAD_CURRENT            0x310D
#define EPEVER_REG_BATTERY_STATUS          0x3200

// EPEVER holding registers; FC03 read, FC10 block-write.
#define EPEVER_REG_BATTERY_TYPE            0x9000
#define EPEVER_REG_RATED_VOLTAGE           0x9067
#define EPEVER_REG_VOLTAGE_BLOCK_START     0x9003
#define EPEVER_VOLTAGE_BLOCK_COUNT         12
#define EPEVER_REG_LOW_VOLTAGE_RECONNECT   0x900A
#define EPEVER_REG_LOW_VOLTAGE_DISCONNECT  0x900D

static HardwareSerial rs485Port(2);

static uint16_t modbusCrc(const uint8_t *bytes, size_t size) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < size; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 1U) ? uint16_t((crc >> 1) ^ 0xA001U) : uint16_t(crc >> 1);
    }
  }
  return crc;
}

static void discardOldRx() {
  while (rs485Port.available()) (void)rs485Port.read();
}

static void sendFrame(uint8_t *frame, size_t payloadLength) {
  const uint16_t crc = modbusCrc(frame, payloadLength);
  frame[payloadLength] = uint8_t(crc & 0xFF);
  frame[payloadLength + 1] = uint8_t(crc >> 8);
  discardOldRx();
  rs485Port.write(frame, payloadLength + 2);
  rs485Port.flush();
}

// Read exactly one validated RTU response; recognize an exception (5 bytes).
static bool receiveFrame(uint8_t functionCode, uint8_t *frame,
                         size_t normalLength, size_t &received) {
  received = 0;
  const uint32_t start = millis();
  while (millis() - start < MODBUS_TIMEOUT_MS) {
    if (!rs485Port.available()) continue;
    const uint8_t b = uint8_t(rs485Port.read());
    if (received >= normalLength) return false;
    frame[received++] = b;
    if (received == 2 && (frame[0] != SENSOR_ADDRESS ||
                          (frame[1] != functionCode &&
                           frame[1] != uint8_t(functionCode | 0x80)))) {
      return false;
    }
    const size_t needed =
        (received >= 2 && frame[1] == uint8_t(functionCode | 0x80))
        ? 5 : normalLength;
    if (received == needed) {
      const uint16_t crc = modbusCrc(frame, needed - 2);
      if (frame[needed - 2] != uint8_t(crc) ||
          frame[needed - 1] != uint8_t(crc >> 8)) return false;
      if (frame[1] & 0x80) {
        Serial.printf("Modbus exception: function 0x%02X, code 0x%02X\n",
                      functionCode, frame[2]);
        return false;
      }
      return true;
    }
  }
  Serial.printf("Modbus timeout: FC%02X, bytes=%u\n", functionCode,
                unsigned(received));
  return false;
}

// FC03: holding registers; FC04: input registers. Count 1..12.
static bool readRegisters(uint8_t functionCode, uint16_t startReg,
                          uint16_t *out, uint8_t count) {
  if ((functionCode != 0x03 && functionCode != 0x04) ||
      count == 0 || count > 12) return false;
  uint8_t request[8] = {SENSOR_ADDRESS, functionCode,
                        uint8_t(startReg >> 8), uint8_t(startReg),
                        0x00, count, 0, 0};
  sendFrame(request, 6);
  uint8_t response[29] = {};
  size_t received = 0;
  const size_t normalLen = 5 + 2 * count;
  if (!receiveFrame(functionCode, response, normalLen, received) ||
      response[2] != uint8_t(count * 2)) return false;
  for (uint8_t i = 0; i < count; ++i) {
    out[i] = (uint16_t(response[3 + 2 * i]) << 8) | response[4 + 2 * i];
  }
  return true;
}

// FC10: write all twelve mutually constrained voltage parameters together.
static bool writeVoltageBlock(const uint16_t *values) {
  uint8_t request[33] = {};
  request[0] = SENSOR_ADDRESS;
  request[1] = 0x10;
  request[2] = uint8_t(EPEVER_REG_VOLTAGE_BLOCK_START >> 8);
  request[3] = uint8_t(EPEVER_REG_VOLTAGE_BLOCK_START);
  request[4] = 0;
  request[5] = EPEVER_VOLTAGE_BLOCK_COUNT;
  request[6] = EPEVER_VOLTAGE_BLOCK_COUNT * 2;
  for (uint8_t i = 0; i < EPEVER_VOLTAGE_BLOCK_COUNT; ++i) {
    request[7 + i * 2] = uint8_t(values[i] >> 8);
    request[8 + i * 2] = uint8_t(values[i]);
  }
  sendFrame(request, 31);
  uint8_t response[8] = {};
  size_t received = 0;
  if (!receiveFrame(0x10, response, sizeof(response), received)) return false;
  return response[2] == request[2] && response[3] == request[3] &&
         response[4] == request[4] && response[5] == request[5];
}

static uint16_t hundredths(float volts) {
  return uint16_t(volts * 100.0f + 0.5f);
}

static bool setpointsAreOrdered(const uint16_t *p) {
  return p[0] > p[1] && p[1] >= p[3] && p[3] >= p[4] &&
         p[4] >= p[5] && p[5] > p[6] && p[0] > p[2] &&
         p[7] > p[10] && p[10] >= p[11] &&
         p[8] > p[9] && p[9] >= p[11] && p[6] > p[10];
}

static void configureProtectionOnce() {
  uint16_t type = 0xFFFF, rated = 0xFFFF;
  if (!readRegisters(0x03, EPEVER_REG_BATTERY_TYPE, &type, 1) ||
      !readRegisters(0x03, EPEVER_REG_RATED_VOLTAGE, &rated, 1)) {
    Serial.println("CONFIG ABORT: cannot verify battery type/rated voltage.");
    return;
  }
  // User-defined battery=0; rated-voltage code=1 means 12V.
  if (type != 0 || rated != 1) {
    Serial.printf("CONFIG ABORT: expected USER/12V, got type=%u rated=%u\n",
                  unsigned(type), unsigned(rated));
    return;
  }

  const uint16_t desired[EPEVER_VOLTAGE_BLOCK_COUNT] = {
    hundredths(PROFILE_OVER_VOLTAGE_DISCONNECT_V), // 0x9003
    hundredths(PROFILE_CHARGING_LIMIT_V),          // 0x9004
    hundredths(PROFILE_OVER_VOLTAGE_RECONNECT_V),  // 0x9005
    hundredths(PROFILE_EQUALIZATION_V),            // 0x9006
    hundredths(PROFILE_BOOST_V),                   // 0x9007
    hundredths(PROFILE_FLOAT_V),                   // 0x9008
    hundredths(PROFILE_BOOST_RECONNECT_V),         // 0x9009
    hundredths(PROFILE_LOW_VOLTAGE_RECONNECT_V),   // 0x900A
    hundredths(PROFILE_UNDER_VOLTAGE_RECOVER_V),   // 0x900B
    hundredths(PROFILE_UNDER_VOLTAGE_WARNING_V),   // 0x900C
    hundredths(PROFILE_LOW_VOLTAGE_DISCONNECT_V),  // 0x900D
    hundredths(PROFILE_DISCHARGING_LIMIT_V)        // 0x900E
  };
  if (!setpointsAreOrdered(desired)) {
    Serial.println("CONFIG ABORT: incompatible voltage setpoints.");
    return;
  }

  uint16_t existing[EPEVER_VOLTAGE_BLOCK_COUNT] = {};
  if (!readRegisters(0x03, EPEVER_REG_VOLTAGE_BLOCK_START,
                     existing, EPEVER_VOLTAGE_BLOCK_COUNT)) {
    Serial.println("CONFIG ABORT: cannot read current voltage block.");
    return;
  }
  bool differs = false;
  for (uint8_t i = 0; i < EPEVER_VOLTAGE_BLOCK_COUNT; ++i) {
    if (existing[i] != desired[i]) differs = true;
  }
  if (!differs) {
    Serial.println("CONFIG OK: battery voltage profile already matches.");
    return;
  }
  if (!writeVoltageBlock(desired)) {
    Serial.println("CONFIG FAILED: FC10 write was rejected or timed out.");
    return;
  }
  delay(250);
  uint16_t verified[EPEVER_VOLTAGE_BLOCK_COUNT] = {};
  if (!readRegisters(0x03, EPEVER_REG_VOLTAGE_BLOCK_START,
                     verified, EPEVER_VOLTAGE_BLOCK_COUNT)) {
    Serial.println("CONFIG UNCERTAIN: readback failed after write.");
    return;
  }
  for (uint8_t i = 0; i < EPEVER_VOLTAGE_BLOCK_COUNT; ++i) {
    if (verified[i] != desired[i]) {
      Serial.printf("CONFIG FAILED: 0x%04X readback %u != target %u\n",
                    EPEVER_REG_VOLTAGE_BLOCK_START + i,
                    unsigned(verified[i]), unsigned(desired[i]));
      return;
    }
  }
  Serial.println("CONFIG VERIFIED: charge limit 14.40V, LOAD OFF 11.80V / ON 12.50V.");
  Serial.println("NOTE: LOAD voltage is NOT regulated to a maximum of 14.4V.");
}

static bool readBatteryVoltage(float &value) {
  uint16_t raw = 0;
  if (!readRegisters(0x04, EPEVER_REG_BATTERY_VOLTAGE, &raw, 1))
    return false;
  value = raw * 0.01f;
  return true;
}

static bool readBatteryTemperature(float &value) {
  uint16_t raw = 0;
  if (!readRegisters(0x04, EPEVER_REG_BATTERY_TEMPERATURE, &raw, 1))
    return false;
  value = int16_t(raw) * 0.01f;
  return true;
}

static bool readLoadCurrent(float &value) {
  uint16_t raw = 0;
  if (!readRegisters(0x04, EPEVER_REG_LOAD_CURRENT, &raw, 1))
    return false;
  value = raw * 0.01f;
  return true;
}

static bool readBatteryStatus(uint16_t &status) {
  return readRegisters(0x04, EPEVER_REG_BATTERY_STATUS, &status, 1);
}

void setup() {
  Serial.begin(115200);
  rs485Port.begin(SENSOR_RS485_BAUD, SERIAL_8N1,
                  RS485_RX_PIN, RS485_TX_PIN);
  Serial.println("EPEVER LS1024B: 12V/9Ah, slave 0x60, 115200 8N1");
#if APPLY_BATTERY_PROFILE_ON_BOOT
  Serial.println("CAUTION: attempting one-time battery-profile write.");
  configureProtectionOnce();
#else
  Serial.println("READ ONLY: profile write disabled; controller settings unchanged.");
#endif
}

void loop() {
  static uint32_t lastPoll = 0;
  if (lastPoll != 0 && millis() - lastPoll < POLL_INTERVAL_MS) return;
  lastPoll = millis();

  float voltage = 0.0f, temperature = 0.0f, loadCurrent = 0.0f;
  uint16_t batteryStatus = 0;
  const bool voltsOk = readBatteryVoltage(voltage);
  const bool tempOk = readBatteryTemperature(temperature);
  const bool loadOk = readLoadCurrent(loadCurrent);
  const bool statusOk = readBatteryStatus(batteryStatus);
  if (voltsOk) {
    Serial.printf("Batt: %.2f V", voltage);
    if (voltage > 14.4f)
      Serial.print(" [ABOVE 14.4V: check load input rating; DC-DC regulator needed]");
    Serial.println();
  } else {
    Serial.println("Battery voltage read failed.");
  }
  if (loadOk) {
    Serial.printf("Load current: %.2f A\n", loadCurrent);
  } else {
    Serial.println("Load current read failed.");
  }
  if (statusOk) {
    Serial.printf("Battery status: 0x%04X\n", unsigned(batteryStatus));
  } else {
    Serial.println("Battery status read failed.");
  }
  if (tempOk) {
    Serial.printf("BattTemp: %.2f C (verify external probe location)\n", temperature);
  } else {
    Serial.println("Battery temperature read failed.");
  }
}
