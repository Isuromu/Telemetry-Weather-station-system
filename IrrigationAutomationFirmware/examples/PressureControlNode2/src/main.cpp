#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "ValveLoRaWan.h"

// ========================= PINLAR =========================
namespace Pin {
constexpr uint8_t VALVE_IN1   = 2;
constexpr uint8_t VALVE_IN2   = 15;
constexpr uint8_t L298_POWER  = 27;
constexpr uint8_t BATTERY_ADC = 35;

constexpr uint8_t RS485_RX = 16;
constexpr uint8_t RS485_TX = 17;

constexpr uint8_t I2C1_SDA = 21;  // Klapandan oldingi sensor
constexpr uint8_t I2C1_SCL = 22;

constexpr uint8_t I2C2_SDA = 13;  // Klapandan keyingi sensor
constexpr uint8_t I2C2_SCL = 4;
}

// ====================== XDB401 SOZLAMALARI ======================
// Sensor: 0–1 MPa = 0–10 bar
constexpr float SENSOR_FULL_SCALE_BAR = 10.0F;

constexpr uint32_t SERIAL_MONITOR_BAUD = 115200;
// Vaqtinchalik sozlama: RS485 qurilmasining tezligiga moslash kerak.
constexpr uint32_t RS485_BAUD = 9600;

// XDB401 turli partiyalarda shu manzillardan birida bo‘lishi mumkin.
constexpr uint8_t XDB401_ADDRESS_PRIMARY   = 0x7F;
constexpr uint8_t XDB401_ADDRESS_ALTERNATE = 0x6D;

constexpr uint8_t REG_PRESSURE    = 0x06;
constexpr uint8_t REG_TEMPERATURE = 0x09;
constexpr uint8_t REG_MEASURE     = 0x30;
constexpr uint8_t CMD_MEASURE     = 0x0A;

// ===================== BATAREYA BO‘LGICHI =====================
// Battery+ -> 100k -> GPIO35 -> 22k -> GND
constexpr float DIVIDER_HIGH_OHM = 100000.0F;
constexpr float DIVIDER_LOW_OHM  = 22000.0F;

// ADC ko‘rsatishini multimeter bilan solishtirib tuzatish mumkin.
constexpr float BATTERY_CALIBRATION = 1.0F;

// ========================== OBYEKTLAR ==========================
TwoWire i2cBefore(0);
TwoWire i2cAfter(1);

uint8_t addressBefore = 0;
uint8_t addressAfter  = 0;

void executeCommand(String input);
size_t buildLoRaStatus(uint8_t *payload, size_t capacity);
ValveLoRaWan lorawan(executeCommand, buildLoRaStatus);

String serialCommand;
bool valveIsOpen = false;
uint32_t lastReportTime = 0;

struct SensorReading {
  bool valid;
  float pressureBar;
  float temperatureC;
};

// ====================== I2C FUNKSIYALARI ======================
bool devicePresent(TwoWire &bus, uint8_t address) {
  bus.beginTransmission(address);
  return bus.endTransmission() == 0;
}

uint8_t findXdb401(TwoWire &bus) {
  if (devicePresent(bus, XDB401_ADDRESS_PRIMARY)) {
    return XDB401_ADDRESS_PRIMARY;
  }

  if (devicePresent(bus, XDB401_ADDRESS_ALTERNATE)) {
    return XDB401_ADDRESS_ALTERNATE;
  }

  return 0;
}

bool writeRegister(
    TwoWire &bus,
    uint8_t address,
    uint8_t reg,
    uint8_t value
) {
  bus.beginTransmission(address);
  bus.write(reg);
  bus.write(value);

  return bus.endTransmission() == 0;
}

bool readRegister(
    TwoWire &bus,
    uint8_t address,
    uint8_t reg,
    uint8_t *data,
    size_t length
) {
  bus.beginTransmission(address);
  bus.write(reg);

  // false = repeated start
  if (bus.endTransmission(false) != 0) {
    return false;
  }

  const size_t received = bus.requestFrom(
      address,
      static_cast<uint8_t>(length)
  );

  if (received != length) {
    return false;
  }

  for (size_t i = 0; i < length; ++i) {
    data[i] = bus.read();
  }

  return true;
}

SensorReading readXdb401(TwoWire &bus, uint8_t address) {
  SensorReading result = {false, 0.0F, 0.0F};

  if (address == 0) {
    return result;
  }

  // Yangi o‘lchovni boshlash.
  if (!writeRegister(bus, address, REG_MEASURE, CMD_MEASURE)) {
    return result;
  }

  // Sensor tayyor bo‘lishini kutish.
  bool measurementReady = false;

  for (uint8_t attempt = 0; attempt < 10; ++attempt) {
    delay(5);

    uint8_t status = 0;

    if (!readRegister(bus, address, REG_MEASURE, &status, 1)) {
      return result;
    }

    // 3-bit nol bo‘lsa, o‘lchov tayyor.
    if ((status & 0x08) == 0) {
      measurementReady = true;
      break;
    }
  }

  if (!measurementReady) {
    return result;
  }

  uint8_t pressureData[3]    = {0};
  uint8_t temperatureData[2] = {0};

  if (!readRegister(
          bus,
          address,
          REG_PRESSURE,
          pressureData,
          sizeof(pressureData)
      )) {
    return result;
  }

  if (!readRegister(
          bus,
          address,
          REG_TEMPERATURE,
          temperatureData,
          sizeof(temperatureData)
      )) {
    return result;
  }

  // 24-bit big-endian signed bosim qiymati.
  uint32_t raw24 =
      (static_cast<uint32_t>(pressureData[0]) << 16) |
      (static_cast<uint32_t>(pressureData[1]) << 8)  |
      static_cast<uint32_t>(pressureData[2]);

  int32_t rawPressure;

  // 24-bit qiymatni 32-bit signed qiymatga kengaytirish.
  if (raw24 & 0x800000UL) {
    rawPressure = static_cast<int32_t>(raw24 | 0xFF000000UL);
  } else {
    rawPressure = static_cast<int32_t>(raw24);
  }

  // 16-bit big-endian signed harorat qiymati.
  int16_t rawTemperature = static_cast<int16_t>(
      (static_cast<uint16_t>(temperatureData[0]) << 8) |
      static_cast<uint16_t>(temperatureData[1])
  );

  result.pressureBar =
      static_cast<float>(rawPressure) /
      8388608.0F *
      SENSOR_FULL_SCALE_BAR;

  result.temperatureC =
      static_cast<float>(rawTemperature) / 256.0F;

  result.valid = true;
  return result;
}

// ====================== BATAREYA O‘LCHOVI ======================
float readBatteryVoltage() {
  constexpr uint8_t SAMPLE_COUNT = 32;

  uint32_t millivoltSum = 0;

  for (uint8_t i = 0; i < SAMPLE_COUNT; ++i) {
    millivoltSum += analogReadMilliVolts(Pin::BATTERY_ADC);
    delayMicroseconds(200);
  }

  const float adcVoltage =
      (millivoltSum / static_cast<float>(SAMPLE_COUNT)) / 1000.0F;

  const float batteryVoltage =
      adcVoltage *
      (DIVIDER_HIGH_OHM + DIVIDER_LOW_OHM) /
      DIVIDER_LOW_OHM;

  return batteryVoltage * BATTERY_CALIBRATION;
}

// ====================== KLAPAN BOSHQARUVI ======================
void closeValve() {
  // Avval L298N chiqishini o‘chiramiz.
  digitalWrite(Pin::VALVE_IN1, LOW);
  digitalWrite(Pin::VALVE_IN2, LOW);

  // So‘ng IRF4905 quvvat kalitini o‘chiramiz.
  digitalWrite(Pin::L298_POWER, LOW);

  valveIsOpen = false;

  if (Serial) {
    Serial.println("OK: valve CLOSED");
  }
}

void openValve() {
  // Xavfsiz boshlang‘ich holat.
  digitalWrite(Pin::VALVE_IN1, LOW);
  digitalWrite(Pin::VALVE_IN2, LOW);

  // GPIO27 HIGH -> BC547 ochiladi -> IRF4905 ochiladi.
  digitalWrite(Pin::L298_POWER, HIGH);
  delay(20);

  // Solenoidga tok berish.
  digitalWrite(Pin::VALVE_IN1, HIGH);
  digitalWrite(Pin::VALVE_IN2, LOW);

  valveIsOpen = true;
  Serial.println("OK: valve OPEN");
}

// ======================== STATUS CHIQARISH ========================
void printSensorReading(
    const char *name,
    const SensorReading &reading,
    uint8_t address
) {
  if (address == 0) {
    Serial.printf("%s: SENSOR NOT FOUND\n", name);
    return;
  }

  if (!reading.valid) {
    Serial.printf(
        "%s: READ ERROR, address 0x%02X\n",
        name,
        address
    );
    return;
  }

  Serial.printf(
      "%s: %.3f bar | %.2f C | address 0x%02X\n",
      name,
      reading.pressureBar,
      reading.temperatureC,
      address
  );
}

void printStatus() {
  const SensorReading before =
      readXdb401(i2cBefore, addressBefore);

  const SensorReading after =
      readXdb401(i2cAfter, addressAfter);

  Serial.println();
  Serial.println("========== STATUS ==========");

  Serial.printf(
      "Valve: %s\n",
      valveIsOpen ? "OPEN" : "CLOSED"
  );

  Serial.printf(
      "Battery: %.2f V\n",
      readBatteryVoltage()
  );

  printSensorReading(
      "Pressure BEFORE",
      before,
      addressBefore
  );

  printSensorReading(
      "Pressure AFTER ",
      after,
      addressAfter
  );

  if (before.valid && after.valid) {
    const float pressureDifference =
        before.pressureBar - after.pressureBar;

    Serial.printf(
        "Pressure DIFFERENCE: %.3f bar\n",
        pressureDifference
    );
  }

  Serial.println("============================");
}

// ======================= SERIAL BUYRUQLAR =======================
void executeCommand(String input) {
  input.trim();
  input.toLowerCase();

  if (input == "open") {
    openValve();
    lorawan.requestStatus();
  } else if (input == "close") {
    closeValve();
    lorawan.requestStatus();
  } else if (input == "status") {
    printStatus();
    lorawan.requestStatus();
  } else if (input == "help" || input == "?") {
    Serial.println("Commands:");
    Serial.println("  open   - open valve");
    Serial.println("  close  - close valve");
    Serial.println("  status - show all measurements");
    Serial.println("  help   - show commands");
  } else if (!input.isEmpty()) {
    Serial.printf("Unknown command: %s\n", input.c_str());
    Serial.println("Type: help");
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    const char character =
        static_cast<char>(Serial.read());

    if (character == '\r' || character == '\n') {
      if (!serialCommand.isEmpty()) {
        executeCommand(serialCommand);
        serialCommand = "";
      }
    } else if (serialCommand.length() < 40) {
      serialCommand += character;
    }
  }
}

// ============================ SETUP ============================
void setup() {
  // ESP32 ishga tushishi bilan klapan yopiq turadi.
  pinMode(Pin::VALVE_IN1, OUTPUT);
  pinMode(Pin::VALVE_IN2, OUTPUT);
  pinMode(Pin::L298_POWER, OUTPUT);

  digitalWrite(Pin::VALVE_IN1, LOW);
  digitalWrite(Pin::VALVE_IN2, LOW);
  digitalWrite(Pin::L298_POWER, LOW);
  valveIsOpen = false;

  Serial.begin(SERIAL_MONITOR_BAUD);
  // UART2 tayyorlanadi. RS485 protokoli va DE/RE boshqaruvi hali kiritilmagan.
  // Hozir buyruqlar USB Serial Monitor orqali qabul qilinadi.
  Serial2.begin(RS485_BAUD, SERIAL_8N1, Pin::RS485_RX, Pin::RS485_TX);
  delay(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(Pin::BATTERY_ADC, ADC_11db);

  // Ikki mustaqil I2C shina.
  i2cBefore.begin(
      Pin::I2C1_SDA,
      Pin::I2C1_SCL,
      100000
  );

  i2cAfter.begin(
      Pin::I2C2_SDA,
      Pin::I2C2_SCL,
      100000
  );

  serialCommand.reserve(40);

  addressBefore = findXdb401(i2cBefore);
  addressAfter  = findXdb401(i2cAfter);

  Serial.println();
  Serial.println("XDB401 valve controller ready");
  Serial.println("Sensor range: 0-1 MPa / 0-10 bar");

  if (addressBefore != 0) {
    Serial.printf(
        "BEFORE sensor found: 0x%02X\n",
        addressBefore
    );
  } else {
    Serial.println("BEFORE sensor NOT FOUND");
  }

  if (addressAfter != 0) {
    Serial.printf(
        "AFTER sensor found: 0x%02X\n",
        addressAfter
    );
  } else {
    Serial.println("AFTER sensor NOT FOUND");
  }

  Serial.println("Commands: open, close, status, help");

  printStatus();
  lorawan.begin();
}

// ============================= LOOP =============================
void loop() {
  readSerialCommands();
  lorawan.poll();

  // Har 2 soniyada avtomatik ma’lumot chiqarish.
  if (millis() - lastReportTime >= 2000) {
    lastReportTime = millis();
    printStatus();
  }
}
// Payload v1: flags, battery mV, two signed pressures (mbar), temperatures (0.01 C).
// Sensor validity bits distinguish missing/error readings from a real zero.
size_t buildLoRaStatus(uint8_t *payload, size_t capacity) {
  if (capacity < 12) return 0;
  const SensorReading before = readXdb401(i2cBefore, addressBefore);
  const SensorReading after = readXdb401(i2cAfter, addressAfter);
  auto put16 = [payload](size_t offset, uint16_t value) {
    payload[offset] = value >> 8;
    payload[offset + 1] = value & 0xff;
  };
  auto scaled = [](float value, float scale) -> int16_t {
    if (!isfinite(value)) return 0;
    const float rounded = roundf(value * scale);
    return static_cast<int16_t>(fmaxf(-32768.0F, fminf(32767.0F, rounded)));
  };
  payload[0] = 1;
  payload[1] = (valveIsOpen ? 1 : 0) | (before.valid ? 2 : 0) | (after.valid ? 4 : 0);
  const float mv = roundf(readBatteryVoltage() * 1000.0F);
  put16(2, static_cast<uint16_t>(fmaxf(0.0F, fminf(65535.0F, mv))));
  put16(4, static_cast<uint16_t>(scaled(before.pressureBar, 1000.0F)));
  put16(6, static_cast<uint16_t>(scaled(after.pressureBar, 1000.0F)));
  put16(8, static_cast<uint16_t>(scaled(before.temperatureC, 100.0F)));
  put16(10, static_cast<uint16_t>(scaled(after.temperatureC, 100.0F)));
  return 12;
}
