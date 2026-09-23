#include <Arduino.h>
#include <Preferences.h>
#include <RadioLib.h>
#include <SPI.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "WaterLevelConfig.h"
#include "WaterLevelLoRaProtocol.h"

#if __has_include("WaterLevelLoRaSecrets.h")
#include "WaterLevelLoRaSecrets.h"
#else
#include "WaterLevelLoRaSecrets.example.h"
#endif

namespace {

namespace config = irrigation::water_level::config;
namespace lora_protocol = irrigation::water_level::lorawan_protocol;
namespace lora_secrets = irrigation::water_level::lorawan_secrets;

HardwareSerial rs485(2);

const LoRaWANBand_t loraWanRegion = EU868;
SPISettings loraSpiSettings(500000, MSBFIRST, SPI_MODE0);
SX1262 radio = new Module(
    config::pins::LORA_NSS, config::pins::LORA_DIO1,
    config::pins::LORA_RESET, config::pins::LORA_BUSY, SPI, loraSpiSettings);
LoRaWANNode lorawan(&radio, &loraWanRegion, config::lorawan::SUB_BAND);
Preferences preferences;

inline constexpr uint32_t RTC_SESSION_MAGIC_VALUE = 0x574C5331UL;
RTC_DATA_ATTR uint32_t rtcSessionMagic = 0;
RTC_DATA_ATTR uint8_t
    rtcLoRaWanSession[RADIOLIB_LORAWAN_SESSION_BUF_SIZE] = {0};
RTC_DATA_ATTR bool retainedLoadStateValid = false;
RTC_DATA_ATTR bool retainedLoadOn = false;

bool radioInitialized = false;

uint16_t crc16Modbus(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; ++i) {
    crc ^= static_cast<uint16_t>(data[i]);
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x0001) != 0 ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc;
}

void setRs485Direction(bool transmit) {
  if (config::pins::RS485_DE_RE < 0) return;
  digitalWrite(config::pins::RS485_DE_RE, transmit ? HIGH : LOW);
}

bool modbusReadRegister(uint16_t registerAddress, int16_t &value) {
  uint8_t request[8] = {
      config::sensor::MODBUS_ID,
      config::sensor::MODBUS_FUNCTION,
      static_cast<uint8_t>(registerAddress >> 8),
      static_cast<uint8_t>(registerAddress & 0xFF),
      static_cast<uint8_t>(config::sensor::MODBUS_REGISTER_COUNT >> 8),
      static_cast<uint8_t>(config::sensor::MODBUS_REGISTER_COUNT & 0xFF),
      0x00,
      0x00,
  };

  const uint16_t crc = crc16Modbus(request, 6);
  request[6] = static_cast<uint8_t>(crc & 0xFF);
  request[7] = static_cast<uint8_t>(crc >> 8);

  while (rs485.available()) rs485.read();

  setRs485Direction(true);
  delayMicroseconds(500);
  rs485.write(request, sizeof(request));
  rs485.flush();
  delayMicroseconds(500);
  setRs485Direction(false);

  uint8_t response[7] = {};
  size_t received = 0;
  const uint32_t startedAt = millis();
  while (millis() - startedAt < config::sensor::RESPONSE_TIMEOUT_MS) {
    while (rs485.available() && received < sizeof(response)) {
      response[received++] = static_cast<uint8_t>(rs485.read());
    }
    if (received == sizeof(response)) break;
    delay(5);
  }

  if (received != sizeof(response)) {
    Serial.printf("RS485: no complete response for register 0x%04X\n",
                  registerAddress);
    return false;
  }

  const uint16_t receivedCrc = static_cast<uint16_t>(response[5]) |
                               (static_cast<uint16_t>(response[6]) << 8);

  if (response[0] != config::sensor::MODBUS_ID ||
      response[1] != config::sensor::MODBUS_FUNCTION || response[2] != 2) {
    Serial.printf("RS485: invalid Modbus response format for register 0x%04X\n",
                  registerAddress);
    return false;
  }

  if (crc16Modbus(response, 5) != receivedCrc) {
    Serial.printf("RS485: CRC error for register 0x%04X\n", registerAddress);
    return false;
  }

  value = static_cast<int16_t>(
      (static_cast<uint16_t>(response[3]) << 8) | response[4]);
  return true;
}

// REG0002 "Primary variable unit" table, expressed as pascal per unit.
constexpr float PA_PER_UNIT[] = {
    1.0e6F,     // 0 - MPa
    1.0e3F,     // 1 - kPa
    1.0F,       // 2 - Pa
    1.0e5F,     // 3 - bar
    1.0e2F,     // 4 - mbar
    98066.5F,   // 5 - kg/cm2
    6894.757F,  // 6 - psi
    9806.65F,   // 7 - mH2O
    9.80665F,   // 8 - mmH2O
};

const char *const UNIT_NAMES[] = {
    "MPa", "kPa", "Pa", "bar", "mbar", "kg/cm2", "psi", "mH2O", "mmH2O",
};

// Reads REG0002/REG0003 to learn how to interpret REG0004, then converts the
// primary variable into metres of water column.
bool readWaterLevel(float &depthMeters) {
  int16_t unit = config::sensor::DEFAULT_UNIT;
  int16_t decimals = config::sensor::DEFAULT_DECIMALS;
  int16_t raw = 0;

  if (!modbusReadRegister(config::sensor::MODBUS_REGISTER_UNIT, unit) ||
      unit < 0 || unit > 8) {
    Serial.println(
        "RS485: unit register unavailable; assuming the datasheet example");
    unit = config::sensor::DEFAULT_UNIT;
  }
  if (!modbusReadRegister(config::sensor::MODBUS_REGISTER_DECIMALS, decimals) ||
      decimals < 0 || decimals > 3) {
    Serial.println(
        "RS485: decimal register unavailable; assuming the datasheet example");
    decimals = config::sensor::DEFAULT_DECIMALS;
  }
  if (!modbusReadRegister(config::sensor::MODBUS_REGISTER_VALUE, raw)) {
    return false;
  }

  float divisor = 1.0F;
  for (int16_t i = 0; i < decimals; ++i) divisor *= 10.0F;
  const float value = static_cast<float>(raw) / divisor;

  depthMeters = value * PA_PER_UNIT[unit] /
                (config::sensor::WATER_DENSITY_KG_M3 *
                 config::sensor::GRAVITY_M_S2);
  Serial.printf("RS485: unit=%d (%s) decimals=%d raw=%d -> %.3f\n", unit,
                UNIT_NAMES[unit], decimals, raw, value);
  return true;
}

float readBatteryVoltage() {
  const int adcMilliVolts = analogReadMilliVolts(config::pins::BATTERY_ADC);
  const float adcVoltage = adcMilliVolts / 1000.0F;
  return adcVoltage *
         (config::battery::DIVIDER_HIGH_OHM +
          config::battery::DIVIDER_LOW_OHM) /
         config::battery::DIVIDER_LOW_OHM * config::battery::CALIBRATION;
}

bool updateLoadControl(float batteryVoltage/*, float levelPercent*/) {
  const bool loadOn =
      batteryVoltage >= config::battery::LOW_VOLTAGE /*&& levelPercent >= 15.0F*/;
  digitalWrite(config::pins::LOAD_CONTROL, loadOn ? HIGH : LOW);
  Serial.println(loadOn ? "LOAD ON"
                        : "LOAD OFF: battery or water level is low");
  return loadOn;
}

void initializeLoadOutput(bool wokeFromDeepSleep) {
  pinMode(config::pins::LOAD_CONTROL, OUTPUT);
  digitalWrite(config::pins::LOAD_CONTROL,
               wokeFromDeepSleep && retainedLoadStateValid && retainedLoadOn
                   ? HIGH
                   : LOW);
  gpio_hold_dis(static_cast<gpio_num_t>(config::pins::LOAD_CONTROL));
  gpio_deep_sleep_hold_dis();
}

lora_protocol::Telemetry runMeasurementAndControlCycle() {
  lora_protocol::Telemetry telemetry{};
  telemetry.batteryVoltage = readBatteryVoltage();
  telemetry.pressureValid = readWaterLevel(telemetry.depthMeters);

  if (telemetry.pressureValid) {
    // The uplink carries pressure in millibar, so recover it from the water
    // column to keep the two reported values consistent.
    telemetry.pressureBar = telemetry.depthMeters *
                            config::sensor::WATER_DENSITY_KG_M3 *
                            config::sensor::GRAVITY_M_S2 / 100000.0F;
    telemetry.levelPercent = constrain(
        telemetry.depthMeters / config::sensor::RANGE_METERS * 100.0F,
        0.0F, 100.0F);
  }

  Serial.printf("Battery: %.2f V\n", telemetry.batteryVoltage);
  Serial.printf("Pressure transmitter: %.3f bar / %.2f kPa\n",
                telemetry.pressureBar, telemetry.pressureBar * 100.0F);
  Serial.printf("Depth: %.2f m\n", telemetry.depthMeters);
  Serial.printf("Water level: %.1f %%\n", telemetry.levelPercent);

  telemetry.loadOn = updateLoadControl(telemetry.batteryVoltage/*, telemetry.levelPercent*/);
  retainedLoadStateValid = true;
  retainedLoadOn = telemetry.loadOn;
  Serial.println("------------------------------------");
  return telemetry;
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

bool setupLoRaWan() {
  SPI.begin(config::pins::LORA_SCK, config::pins::LORA_MISO,
            config::pins::LORA_MOSI, config::pins::LORA_NSS);
  delay(100);

  int16_t state = radio.begin(
      868.0, 125.0, 9, 7, RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 0.0,
      false);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] SX1262 initialization failed: %d\n", state);
    return false;
  }
  radioInitialized = true;
  radio.setRfSwitchPins(config::pins::LORA_RX_ENABLE,
                        config::pins::LORA_TX_ENABLE);

  state = lorawan.beginOTAA(lora_secrets::JOIN_EUI, lora_secrets::DEV_EUI,
                            nullptr, lora_secrets::APP_KEY);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] beginOTAA failed: %d\n", state);
    return false;
  }

  lorawan.setADR(true);
  lorawan.setDeviceStatus(255);

  const bool noncesRestored = loadNoncesFromNvs();
  if (noncesRestored && rtcSessionMagic == RTC_SESSION_MAGIC_VALUE) {
    (void)lorawan.setBufferSession(rtcLoRaWanSession);
  }

  state = lorawan.activateOTAA();
  if (state != RADIOLIB_LORAWAN_NEW_SESSION &&
      state != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    (void)saveNoncesToNvs();
    rtcSessionMagic = 0;
    Serial.printf("[LORAWAN] OTAA activation failed: %d\n", state);
    return false;
  }

  if (state == RADIOLIB_LORAWAN_NEW_SESSION && !saveNoncesToNvs()) {
    Serial.println("[LORAWAN] Warning: OTAA nonces were not saved.");
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

void printHexFrame(const uint8_t *data, size_t length) {
  Serial.print("[LORAWAN] Telemetry FPort 40: ");
  for (size_t i = 0; i < length; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 < length) Serial.print(' ');
  }
  Serial.println();
}

void sendTelemetry(const lora_protocol::Telemetry &telemetry) {
  uint8_t uplink[lora_protocol::TELEMETRY_SIZE] = {};
  lora_protocol::encodeTelemetry(telemetry, uplink);
  printHexFrame(uplink, sizeof(uplink));

  uint8_t downlink[RADIOLIB_LORAWAN_MAX_PAYLOAD_SIZE] = {};
  size_t downlinkLength = sizeof(downlink);
  LoRaWANEvent_t downlinkEvent = {};
  const int16_t state = lorawan.sendReceive(
      uplink, sizeof(uplink), config::lorawan::TELEMETRY_FPORT, downlink,
      &downlinkLength, config::lorawan::CONFIRMED_UPLINK, nullptr,
      &downlinkEvent);
  saveSessionToRtc();

  if (state < RADIOLIB_ERR_NONE) {
    Serial.printf("[LORAWAN] Uplink failed: %d\n", state);
    return;
  }

  Serial.println("[LORAWAN] Telemetry sent; RX1/RX2 completed.");
  if (state > RADIOLIB_ERR_NONE && downlinkLength > 0) {
    Serial.printf(
        "[LORAWAN] Ignored %u-byte application downlink on FPort %u; "
        "remote control is intentionally disabled.\n",
        static_cast<unsigned>(downlinkLength), downlinkEvent.fPort);
  }
}

void enterDeepSleep() {
  if (radioInitialized) {
    const int16_t state = radio.sleep(true);
    if (state != RADIOLIB_ERR_NONE) {
      Serial.printf("[LORAWAN] Radio sleep warning: %d\n", state);
    }
  }
  setRfSwitchOff();
  rs485.end();
  SPI.end();

  gpio_hold_en(static_cast<gpio_num_t>(config::pins::LOAD_CONTROL));
  gpio_deep_sleep_hold_en();
  esp_sleep_enable_timer_wakeup(
      static_cast<uint64_t>(config::lorawan::SLEEP_SECONDS) * 1000000ULL);

  Serial.printf("[POWER] Deep sleeping for %lu seconds; load output is held %s.\n",
                static_cast<unsigned long>(config::lorawan::SLEEP_SECONDS),
                retainedLoadOn ? "ON" : "OFF");
  Serial.flush();
  delay(20);
  esp_deep_sleep_start();
  while (true) delay(1000);
}

}  // namespace

void setup() {
  const bool wokeFromDeepSleep =
      esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
  initializeLoadOutput(wokeFromDeepSleep);
  setRfSwitchOff();

  Serial.begin(115200);
  delay(300);
  Serial.println("====================================");
  Serial.println("ESP32 WaterLevel Class A cycle started");
  Serial.println("====================================");

  pinMode(config::pins::BATTERY_ADC, ANALOG);
  analogSetPinAttenuation(config::pins::BATTERY_ADC, ADC_11db);
  if (config::pins::RS485_DE_RE >= 0) {
    pinMode(config::pins::RS485_DE_RE, OUTPUT);
    digitalWrite(config::pins::RS485_DE_RE, LOW);
  }
  rs485.begin(9600, SERIAL_8N1, config::pins::RS485_RX,
              config::pins::RS485_TX);

  const lora_protocol::Telemetry telemetry =
      runMeasurementAndControlCycle();

  if (!lora_secrets::CONFIGURED) {
    Serial.println(
        "[LORAWAN] WaterLevel OTAA credentials are not configured; "
        "telemetry was not sent.");
    enterDeepSleep();
    return;
  }

  if (setupLoRaWan()) sendTelemetry(telemetry);
  enterDeepSleep();
}

void loop() {
  delay(1000);
}
