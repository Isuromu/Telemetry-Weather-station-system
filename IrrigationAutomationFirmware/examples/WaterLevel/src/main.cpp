#include <Arduino.h>

// =========================
// Suv sathi + batareya monitoring + load kontrol
// ESP32 + RS485 pressure transmitter
// =========================
// Ulanishlar:
// 1) Battery divider:
//    - Battery +  -> 100k -> GPIO35
//    - GPIO35 -> 22k -> GND
//    - Bu formula: Vbat = ADC * 3.3 * (100 + 22) / 22
//
// 2) RS485 modul:
//    - VCC -> 3.3V
//    - GND -> GND
//    - TX  -> GPIO17
//    - RX  -> GPIO16
//    - DE/RE (agar bor bo'lsa) -> GPIO4 yoki boshqa pin
//
// 3) Load / transistor boshqaruv:
//    - GPIO27 -> 1k -> 2N2222 base
//    - 2N2222 emitter -> GND
//    - 2N2222 collector -> load/relay yoki MOSFET gate
//
// 4) "over discharge" signal bo'lsa:
//    - Bu kodda alohida pin ishlatilmaydi, lekin kerak bo'lsa GPIO32 yoki boshqa pin qo'shish mumkin.
// =========================

const int BATTERY_PIN = 35;       // ADC1_CH7
const int LOAD_CONTROL_PIN = 27;  // Load/relay boshqaruv pin
const int RS485_DE_RE_PIN = 255;  // Agar RS485 modulda DE/RE pin mavjud bo'lsa, shu pinni yozing; aks holda 255 qoldiring.
const int RS485_RX_PIN = 16;      // ESP32 RX2
const int RS485_TX_PIN = 17;      // ESP32 TX2

// Divider parametrlari: 100k ust + 22k past
const float R_TOP = 100000.0f;
const float R_BOTTOM = 22000.0f;

// Modbus RTU sozlamalari
// RD pressure transmitterlarning default addressi ko'pincha 1 bo'ladi.
const uint8_t MODBUS_ID = 1;
const uint16_t MODBUS_REG_ADDR = 0x0000;  // Pressure transmitter register manzili
const uint16_t MODBUS_REG_COUNT = 0x0001; // 1 register

// Pressure transmitterning o'lchov koeffitsienti
// Agar transmitterning haqiqiy registri 0..10000 bo'lib, 0..10 bar ni ifodalayotgan bo'lsa:
// pressure_kPa = raw * 0.1f;
// Agar sensor manualida boshqa qiymat bo'lsa, shu qiymatni o'zgartiring.
const float PRESSURE_SCALE = 0.1f; // default: 1 count = 0.1 kPa

// Tank parametrlar
const float TANK_HEIGHT_M = 2.0f;        // Tank balandligi, metrda
const float WATER_DENSITY = 1000.0f;     // kg/m3
const float GRAVITY = 9.81f;              // m/s2

HardwareSerial rs485(2); // RX=GPIO16, TX=GPIO17

uint16_t crc16Modbus(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void setRS485Direction(bool txMode) {
  if (RS485_DE_RE_PIN == 255) {
    return;
  }
  digitalWrite(RS485_DE_RE_PIN, txMode ? HIGH : LOW);
}

bool readPressureFromRS485(float &pressureKPa) {
  // Modbus RTU request: slave + function + register address + register count + CRC
  uint8_t request[8] = {
    MODBUS_ID,
    0x04,
    (uint8_t)(MODBUS_REG_ADDR >> 8),
    (uint8_t)(MODBUS_REG_ADDR & 0xFF),
    (uint8_t)(MODBUS_REG_COUNT >> 8),
    (uint8_t)(MODBUS_REG_COUNT & 0xFF),
    0x00,
    0x00
  };

  uint16_t crc = crc16Modbus(request, 6);
  request[6] = (uint8_t)(crc & 0xFF);
  request[7] = (uint8_t)((crc >> 8) & 0xFF);

  setRS485Direction(true);
  delayMicroseconds(500);
  rs485.write(request, sizeof(request));
  rs485.flush();
  delayMicroseconds(500);
  setRS485Direction(false);

  uint8_t response[7];
  size_t idx = 0;
  unsigned long start = millis();

  while ((millis() - start) < 300) {
    while (rs485.available() && idx < sizeof(response)) {
      response[idx++] = rs485.read();
    }
    if (idx >= sizeof(response)) {
      break;
    }
    delay(5);
  }

  if (idx != sizeof(response)) {
    Serial.println("RS485: javob kelmadi");
    return false;
  }

  uint8_t slave = response[0];
  uint8_t func = response[1];
  uint8_t byteCount = response[2];
  uint16_t raw = (uint16_t)response[3] << 8 | response[4];
  uint16_t receivedCrc = (uint16_t)response[5] | ((uint16_t)response[6] << 8);

  if (slave != MODBUS_ID || func != 0x04 || byteCount != 2) {
    Serial.println("RS485: Modbus javob formati xato");
    return false;
  }

  uint16_t calcCrc = crc16Modbus(response, 5);
  if (calcCrc != receivedCrc) {
    Serial.println("RS485: CRC xato");
    return false;
  }

  pressureKPa = raw * PRESSURE_SCALE;
  return true;
}

float readBatteryVoltage() {
  int raw = analogRead(BATTERY_PIN);
  float vRef = 3.3f;
  float vADC = (raw / 4095.0f) * vRef;
  float vBat = vADC * (R_TOP + R_BOTTOM) / R_BOTTOM;
  return vBat;
}

void updateLoadControl(float batteryVoltage, float levelPercent) {
  // 12V battery uchun over-discharge limit (masalan 11.5V)
  const float LOW_BATTERY_VOLTAGE = 11.5f;

  if (batteryVoltage < LOW_BATTERY_VOLTAGE || levelPercent < 15.0f) {
    digitalWrite(LOAD_CONTROL_PIN, LOW);
    Serial.println("LOAD OFF: batareya past yoki suv sathi past");
  } else {
    digitalWrite(LOAD_CONTROL_PIN, HIGH);
    Serial.println("LOAD ON");
  }
}

void setup() {
  Serial.begin(115200);
  rs485.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  pinMode(BATTERY_PIN, INPUT);
  pinMode(LOAD_CONTROL_PIN, OUTPUT);
  digitalWrite(LOAD_CONTROL_PIN, LOW);

  if (RS485_DE_RE_PIN != 255) {
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    digitalWrite(RS485_DE_RE_PIN, LOW);
  }

  Serial.println("====================================");
  Serial.println("ESP32 Water Level Monitoring Started");
  Serial.println("====================================");
  Serial.println("Battery divider: 100k -> GPIO35 -> 22k -> GND");
  Serial.println("RS485: TX17 RX16");
  Serial.println("Load control: GPIO27 -> 2N2222 base via 1k");
  delay(1000);
}

void loop() {
  float batteryVoltage = readBatteryVoltage();

  float pressureKPa = 0.0f;
  bool pressureOk = readPressureFromRS485(pressureKPa);

  float depthM = 0.0f;
  float levelPercent = 0.0f;

  if (pressureOk) {
    // Suv ustuni uchun bosimdan balandlikni hisoblash.
    // P = rho * g * h => h = P / (rho * g)
    // P kPa ni Pa ga o'tkazish uchun *1000
    depthM = (pressureKPa * 1000.0f) / (WATER_DENSITY * GRAVITY);
    levelPercent = constrain((depthM / TANK_HEIGHT_M) * 100.0f, 0.0f, 100.0f);
  }

  Serial.print("Battery: ");
  Serial.print(batteryVoltage, 2);
  Serial.println(" V");

  Serial.print("Pressure transmitter: ");
  Serial.print(pressureKPa, 2);
  Serial.println(" kPa");

  Serial.print("Depth: ");
  Serial.print(depthM, 2);
  Serial.println(" m");

  Serial.print("Water level: ");
  Serial.print(levelPercent, 1);
  Serial.println(" %");

  updateLoadControl(batteryVoltage, levelPercent);

  Serial.println("------------------------------------");
  delay(2000);
}
