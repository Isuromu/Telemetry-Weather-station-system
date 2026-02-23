#pragma once
#include <Arduino.h>
#include "RS485Modbus.h"

/*
  SoilTR sensor (JXBS-3001-TR 485 "comprehensive soil sensor")

  Registers used (based on your manual photos):
    - Humidity:      0x0012  (0.1 %RH)
    - Temperature:   0x0013  (0.1 C, signed two's complement)
    - Conductivity:  0x0015  (uS/cm)
    - pH:            0x0006  (pH = raw/100)
    - N/P/K block:   0x001E..0x0020 (mg/kg)

  Address change special command (your pattern memory):
    [broadcastAddr] 06 02 00 00 [newAddr] + CRC
    (broadcastAddr often FE or FF depending on sensor family)
*/

struct SoilTR_Data {
  float temperatureC;
  float humidityPct;
  uint16_t conductivity_uScm;
  float pH;
  uint16_t nitrogen_mgkg;
  uint16_t phosphorus_mgkg;
  uint16_t potassium_mgkg;
};

class SoilTR {
public:
  SoilTR(RS485Modbus& bus, uint8_t address);

  void setAddress(uint8_t address);
  uint8_t address() const;

  bool readTemperatureHumidity(float& tempC, float& humPct);
  bool readConductivity(uint16_t& uScm);
  bool readPH(float& ph);
  bool readNPK(uint16_t& n, uint16_t& p, uint16_t& k);
  bool readAll(SoilTR_Data& out);

  bool changeAddressSpecial(uint8_t broadcastAddr, uint8_t newAddress);

private:
  RS485Modbus& _bus;
  uint8_t _addr;

  static int16_t toInt16(uint16_t v);
};
