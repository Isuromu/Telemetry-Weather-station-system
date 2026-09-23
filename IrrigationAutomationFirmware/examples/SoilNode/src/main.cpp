#include <Adafruit_ADS1X15.h>
#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_sleep.h>

#include "SoilNodeConfig.h"
#include "SoilNodeLoRaProtocol.h"

#if __has_include("SoilNodeLoRaSecrets.h")
#include "SoilNodeLoRaSecrets.h"
#else
#include "SoilNodeLoRaSecrets.example.h"
#endif

namespace {

namespace config = irrigation::soil_node::config;
namespace protocol = irrigation::soil_node::lorawan_protocol;
namespace secrets = irrigation::soil_node::lorawan_secrets;

HardwareSerial rs485(1);
Adafruit_ADS1115 ads;
SPISettings loraSpiSettings(2000000, MSBFIRST, SPI_MODE0);
SX1262 radio = new Module(
    config::pins::LORA_NSS, config::pins::LORA_DIO1,
    config::pins::LORA_RESET, config::pins::LORA_BUSY, SPI, loraSpiSettings);
LoRaWANNode lorawan(&radio, &config::lorawan::REGION,
                    config::lorawan::SUB_BAND);
Preferences preferences;

inline constexpr uint32_t RTC_SESSION_MAGIC_VALUE = 0x534E5331UL;
RTC_DATA_ATTR uint32_t rtcSessionMagic = 0;
RTC_DATA_ATTR uint32_t rtcWakeCount = 0;
RTC_DATA_ATTR uint8_t
    rtcLoRaWanSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {};

bool radioInitialized = false;
bool adsInitialized = false;
uint32_t sleepIntervalSeconds = config::lorawan::DEFAULT_SLEEP_SECONDS;

uint16_t crc16Modbus(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x0001) != 0 ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

void printHexFrame(const char *label, const uint8_t *data, size_t length) {
  Serial.print(label);
  for (size_t i = 0; i < length; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 < length) Serial.print(' ');
  }
  Serial.println();
}

void setSensorPower(bool enabled) {
  if (config::pins::SENSOR_POWER < 0) return;
  digitalWrite(config::pins::SENSOR_POWER, enabled ? HIGH : LOW);
  Serial.printf("[POWER] Soil sensor GPIO%d %s.\n",
                config::pins::SENSOR_POWER, enabled ? "ON" : "OFF");
}

bool readSoil(protocol::Telemetry &telemetry) {
  constexpr size_t REQUEST_SIZE = 8;
  constexpr size_t RESPONSE_SIZE = 11;
  constexpr size_t RAW_CAPACITY = 48;
  uint8_t request[REQUEST_SIZE] = {
      config::sensor::MODBUS_ID, 0x03,
      highByte(config::sensor::FIRST_REGISTER),
      lowByte(config::sensor::FIRST_REGISTER),
      highByte(config::sensor::REGISTER_COUNT),
      lowByte(config::sensor::REGISTER_COUNT), 0, 0};
  const uint16_t requestCrc = crc16Modbus(request, REQUEST_SIZE - 2);
  request[6] = lowByte(requestCrc);
  request[7] = highByte(requestCrc);

  if (config::pins::SENSOR_POWER >= 0) {
    setSensorPower(true);
    Serial.printf("[SOIL] Waiting %lu ms for sensor startup.\n",
                  static_cast<unsigned long>(config::sensor::POWER_UP_MS));
    delay(config::sensor::POWER_UP_MS);
  }

  for (uint8_t attempt = 1; attempt <= config::sensor::READ_ATTEMPTS;
       ++attempt) {
    while (rs485.available() > 0) rs485.read();
    Serial.printf("[SOIL] Read attempt %u/%u\n", attempt,
                  config::sensor::READ_ATTEMPTS);
    printHexFrame("[SOIL] TX: ", request, sizeof(request));
    rs485.write(request, sizeof(request));
    rs485.flush();

    uint8_t raw[RAW_CAPACITY] = {};
    size_t received = 0;
    const uint32_t started = millis();
    while (millis() - started < config::sensor::RESPONSE_TIMEOUT_MS) {
      while (rs485.available() > 0) {
        const uint8_t value = static_cast<uint8_t>(rs485.read());
        if (received < sizeof(raw)) raw[received++] = value;
      }
      delay(1);
    }
    printHexFrame("[SOIL] RX: ", raw, received);

    for (size_t start = 0; start + RESPONSE_SIZE <= received; ++start) {
      const uint8_t *frame = raw + start;
      if (frame[0] != config::sensor::MODBUS_ID || frame[1] != 0x03 ||
          frame[2] != 0x06) {
        continue;
      }
      const uint16_t receivedCrc = static_cast<uint16_t>(frame[9]) |
                                   (static_cast<uint16_t>(frame[10]) << 8);
      if (crc16Modbus(frame, RESPONSE_SIZE - 2) != receivedCrc) continue;

      telemetry.temperature100 = static_cast<int16_t>(
          (static_cast<uint16_t>(frame[3]) << 8) | frame[4]);
      telemetry.vwc100 =
          (static_cast<uint16_t>(frame[5]) << 8) | frame[6];
      telemetry.ec1000 =
          (static_cast<uint16_t>(frame[7]) << 8) | frame[8];
      telemetry.sensorValid = true;
      setSensorPower(false);
      return true;
    }

    if (received == REQUEST_SIZE &&
        memcmp(raw, request, REQUEST_SIZE) == 0) {
      Serial.println(
          "[SOIL] Converter echo received, but the sensor did not reply.");
    } else {
      Serial.println("[SOIL] No complete CRC-valid sensor reply found.");
    }
    if (attempt < config::sensor::READ_ATTEMPTS) {
      delay(config::sensor::RETRY_DELAY_MS);
    }
  }

  setSensorPower(false);
  return false;
}

uint8_t readBatteryEncoded() {
  if (!adsInitialized) {
    Serial.println("[BATTERY] ADS1115 unavailable; reporting zero.");
    return 0;
  }

  const int16_t raw = ads.readADC_SingleEnded(config::battery::ADS1115_CHANNEL);
  const float pinVoltage = ads.computeVolts(raw);
  const float batteryVoltage = pinVoltage * config::battery::DIVIDER_RATIO;
  Serial.printf("[BATTERY] ADS A0 %.3f V; battery %.3f V\n",
                pinVoltage, batteryVoltage);

  const float clamped = constrain(batteryVoltage, config::battery::MIN_VOLTS,
                                  config::battery::MAX_VOLTS);
  return static_cast<uint8_t>(
      (clamped - config::battery::ENCODED_OFFSET_VOLTS) * 100.0F);
}

bool loadNoncesFromNvs() {
  if (!preferences.begin(config::lorawan::NVS_NAMESPACE, true)) return false;
  const size_t length =
      preferences.getBytesLength(config::lorawan::NVS_NONCES_KEY);
  if (length != RADIOLIB_LORAWAN_NONCES_BUF_SIZE) {
    preferences.end();
    return false;
  }
  uint8_t nonces[RADIOLIB_LORAWAN_NONCES_BUF_SIZE] = {};
  const size_t read = preferences.getBytes(
      config::lorawan::NVS_NONCES_KEY, nonces, sizeof(nonces));
  preferences.end();
  return read == sizeof(nonces) &&
         lorawan.setBufferNonces(nonces) == RADIOLIB_ERR_NONE;
}

bool saveNoncesToNvs() {
  if (!preferences.begin(config::lorawan::NVS_NAMESPACE, false)) return false;
  const size_t written = preferences.putBytes(
      config::lorawan::NVS_NONCES_KEY, lorawan.getBufferNonces(),
      RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  preferences.end();
  return written == RADIOLIB_LORAWAN_NONCES_BUF_SIZE;
}

void loadSleepIntervalFromNvs() {
  if (!preferences.begin(config::lorawan::NVS_NAMESPACE, true)) return;
  const uint32_t stored = preferences.getUInt(
      config::lorawan::NVS_SLEEP_SECONDS_KEY,
      config::lorawan::DEFAULT_SLEEP_SECONDS);
  preferences.end();
  if (stored >= config::lorawan::MIN_SLEEP_SECONDS &&
      stored <= config::lorawan::MAX_SLEEP_SECONDS) {
    sleepIntervalSeconds = stored;
  }
}

bool saveSleepIntervalToNvs(uint32_t seconds) {
  if (!preferences.begin(config::lorawan::NVS_NAMESPACE, false)) return false;
  const size_t written = preferences.putUInt(
      config::lorawan::NVS_SLEEP_SECONDS_KEY, seconds);
  preferences.end();
  return written == sizeof(seconds);
}

bool processDownlink(const uint8_t *payload, size_t length, uint8_t fPort,
                     protocol::CommandAck &ack) {
  if (fPort != config::lorawan::TELEMETRY_FPORT) {
    Serial.printf("[LORAWAN] Ignored downlink on unexpected FPort %u.\n",
                  fPort);
    return false;
  }

  ack.activeSleepSeconds = sleepIntervalSeconds;
  if (payload != nullptr && length == protocol::SET_SLEEP_INTERVAL_COMMAND_SIZE &&
      payload[0] == protocol::SET_SLEEP_INTERVAL_COMMAND) {
    ack.commandId = (static_cast<uint16_t>(payload[1]) << 8) | payload[2];
  }
  uint32_t requestedSeconds = 0;
  uint16_t commandId = protocol::LEGACY_COMMAND_ID;
  if (!protocol::decodeSleepIntervalCommand(
          payload, length, config::lorawan::MIN_SLEEP_SECONDS,
          config::lorawan::MAX_SLEEP_SECONDS, requestedSeconds, commandId)) {
    Serial.println("[LORAWAN] Invalid sleep-interval downlink.");
    return true;
  }

  ack.commandId = commandId;
  if (requestedSeconds != sleepIntervalSeconds &&
      !saveSleepIntervalToNvs(requestedSeconds)) {
    Serial.println("[LORAWAN] Could not persist the sleep interval.");
    ack.status = protocol::CommandStatus::STORAGE_FAILED;
    return true;
  }
  sleepIntervalSeconds = requestedSeconds;
  ack.status = protocol::CommandStatus::APPLIED;
  ack.activeSleepSeconds = sleepIntervalSeconds;
  Serial.printf("[LORAWAN] Sleep interval set to %lu seconds.\n",
                static_cast<unsigned long>(sleepIntervalSeconds));
  return true;
}

void saveSessionToRtc() {
  memcpy(rtcLoRaWanSession, lorawan.getBufferSession(),
         RADIOLIB_LORAWAN_SESSION_BUF_SIZE);
  rtcSessionMagic = RTC_SESSION_MAGIC_VALUE;
}

void setRfSwitchOff() {
  pinMode(config::pins::LORA_RX_ENABLE, OUTPUT);
  pinMode(config::pins::LORA_TX_ENABLE, OUTPUT);
  digitalWrite(config::pins::LORA_RX_ENABLE, LOW);
  digitalWrite(config::pins::LORA_TX_ENABLE, LOW);
}

bool setupLoRaWan(bool wokeFromDeepSleep) {
  SPI.begin(config::pins::LORA_SCK, config::pins::LORA_MISO,
            config::pins::LORA_MOSI, config::pins::LORA_NSS);
  int16_t state = radio.begin(
      868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 14, 8, 0.0,
      false);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] SX1262 initialization failed: %d\n", state);
    return false;
  }
  radioInitialized = true;
  radio.setRfSwitchPins(config::pins::LORA_RX_ENABLE,
                        config::pins::LORA_TX_ENABLE);

  state = lorawan.beginOTAA(secrets::JOIN_EUI, secrets::DEV_EUI, nullptr,
                            secrets::APP_KEY);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] beginOTAA failed: %d\n", state);
    return false;
  }
  lorawan.setADR(true);

  const bool noncesRestored = loadNoncesFromNvs();
  if (wokeFromDeepSleep && noncesRestored &&
      rtcSessionMagic == RTC_SESSION_MAGIC_VALUE) {
    (void)lorawan.setBufferSession(rtcLoRaWanSession);
  }

  state = lorawan.activateOTAA();
  if (state != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    // A real join attempt changes nonce state even when the join fails.
    if (!saveNoncesToNvs()) {
      Serial.println("[LORAWAN] Cannot safely persist OTAA nonces.");
      return false;
    }
    if (state != RADIOLIB_LORAWAN_NEW_SESSION) {
      Serial.printf("[LORAWAN] OTAA activation failed: %d\n", state);
      rtcSessionMagic = 0;
      return false;
    }
  }

  saveSessionToRtc();
  state = lorawan.setClass(RADIOLIB_LORAWAN_CLASS_A);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] Could not select Class A: %d\n", state);
    return false;
  }
  Serial.println("[LORAWAN] Class A session ready.");
  return true;
}

void enterDeepSleep() {
  setSensorPower(false);
  if (radioInitialized) (void)radio.sleep(true);
  setRfSwitchOff();
  rs485.end();
  SPI.end();
  esp_sleep_enable_timer_wakeup(
      static_cast<uint64_t>(sleepIntervalSeconds) * 1000000ULL);
  Serial.printf("[POWER] Deep sleeping for %lu seconds.\n",
                static_cast<unsigned long>(sleepIntervalSeconds));
  Serial.flush();
  delay(20);
  esp_deep_sleep_start();
  while (true) delay(1000);
}

void sendTelemetry(const protocol::Telemetry &telemetry) {
  uint8_t payload[protocol::TELEMETRY_SIZE] = {};
  protocol::encodeTelemetry(telemetry, payload);
  printHexFrame("[LORAWAN] FPort 10: ", payload, sizeof(payload));

  uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
  size_t downlinkLength = sizeof(downlink);
  LoRaWANEvent_t downlinkEvent = {};
  const int16_t state = lorawan.sendReceive(
      payload, sizeof(payload), config::lorawan::TELEMETRY_FPORT, downlink,
      &downlinkLength, false, nullptr, &downlinkEvent);
  saveSessionToRtc();
  if (state < RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] Uplink failed: %d\n", state);
  } else {
    Serial.println("[LORAWAN] Uplink sent; RX1/RX2 completed.");
    if (state > RADIOLIB_ERR_NONE && downlinkLength > 0) {
      printHexFrame("[LORAWAN] Downlink: ", downlink, downlinkLength);
      protocol::CommandAck ack{};
      if (processDownlink(downlink, downlinkLength, downlinkEvent.fPort, ack)) {
        uint8_t ackPayload[protocol::COMMAND_ACK_SIZE] = {};
        protocol::encodeCommandAck(ack, ackPayload);
        printHexFrame("[LORAWAN] FPort 11 command result: ", ackPayload,
                      sizeof(ackPayload));
        uint8_t ignoredDownlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
        size_t ignoredLength = sizeof(ignoredDownlink);
        LoRaWANEvent_t ignoredEvent = {};
        const int16_t ackState = lorawan.sendReceive(
            ackPayload, sizeof(ackPayload), config::lorawan::COMMAND_ACK_FPORT,
            ignoredDownlink, &ignoredLength, false, nullptr, &ignoredEvent);
        saveSessionToRtc();
        if (ackState < RADIOLIB_ERR_NONE) {
          Serial.printf("[LORAWAN] Application acknowledgement failed: %d\n",
                        ackState);
        } else {
          Serial.println("[LORAWAN] Application acknowledgement sent.");
          if (ackState > RADIOLIB_ERR_NONE && ignoredLength > 0) {
            Serial.println("[LORAWAN] Additional queued downlink was not "
                           "processed. Queue one command at a time.");
          }
        }
      }
    }
  }
}

}  // namespace

void setup() {
  if (config::pins::SENSOR_POWER >= 0) {
    pinMode(config::pins::SENSOR_POWER, OUTPUT);
    setSensorPower(false);
  }
  setRfSwitchOff();

  Serial.begin(115200);
  delay(300);
  const bool wokeFromDeepSleep =
      esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
  rtcWakeCount = wokeFromDeepSleep ? rtcWakeCount + 1 : 0;
  loadSleepIntervalFromNvs();

  Serial.println("ESP32-WROOM SoilNode Class A cycle started");
  Serial.printf("Wake count: %lu\n", static_cast<unsigned long>(rtcWakeCount));
  rs485.setRxBufferSize(256);
  rs485.begin(config::sensor::BAUD, SERIAL_8N1, config::pins::RS485_RX,
              config::pins::RS485_TX);

  Wire.begin(config::pins::I2C_SDA, config::pins::I2C_SCL);
  adsInitialized = ads.begin();
  if (!adsInitialized) Serial.println("[BATTERY] ADS1115 not detected.");

  protocol::Telemetry telemetry{};
  readSoil(telemetry);
  telemetry.batteryEncoded = readBatteryEncoded();
  if (telemetry.sensorValid) {
    Serial.printf("Temperature %.2f C; VWC %.2f %%; EC %.3f mS/cm\n",
                  telemetry.temperature100 / 100.0F,
                  telemetry.vwc100 / 100.0F,
                  telemetry.ec1000 / 1000.0F);
  }

  if (!secrets::CONFIGURED) {
    Serial.println("[LORAWAN] SoilNode OTAA credentials are not configured.");
    enterDeepSleep();
  }

  if (setupLoRaWan(wokeFromDeepSleep)) {
    const uint8_t batteryStatus = map(
        constrain(telemetry.batteryEncoded, 100, 220), 100, 220, 1, 254);
    lorawan.setDeviceStatus(batteryStatus);
    sendTelemetry(telemetry);
  }
  enterDeepSleep();
}

void loop() {
  delay(1000);
}
