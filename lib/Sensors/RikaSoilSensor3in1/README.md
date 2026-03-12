# RikaSoilSensor3in1 Driver Notes

## Purpose
`RikaSoilSensor3in1` is an RS485 Modbus RTU driver for RK520-02 / JXBS-3001-TR class soil sensors.

Primary measurements:
- `soil_temp` (degrees Celsius)
- `soil_vwc` (volumetric water content, %)
- `soil_ec` (electrical conductivity, mS/cm)

Additional values:
- `epsilon` (dielectric constant)
- `currentSoilType` (enum cache of configured soil profile)

## Main Read Transaction (`readData`)
Request:

`[addr] [03] [00] [00] [00] [03] [crc_lo] [crc_hi]`

- Function `0x03`: read holding registers
- Start register `0x0000`
- Register count `0x0003`

Response layout:

`[addr] [03] [06] [temp_hi] [temp_lo] [vwc_hi] [vwc_lo] [ec_hi] [ec_lo] [crc_lo] [crc_hi]`

Scaling:
- temperature = `rawTemp / 10.0` (signed 16-bit)
- VWC = `rawVwc / 10.0`
- EC = `rawEc / 1000.0`

## Validation and Error Handling
- Driver retries: `SENSOR_DEFAULT_DRIVER_RETRIES`.
- Bus retries + CRC validation: inside `RS485Bus::SendRequest`.
- Range checks in driver:
  - temperature: `-40.0 .. 80.0`
  - VWC: `0.0 .. 100.0`
  - EC: `0.0 .. 50.0`
- On total read failure:
  - `setFallbackValues()` sets numeric values to `-99.0` and soil type to `SOIL_UNKNOWN`
  - `markFailure()` updates `SensorDriver` health state

## Soil Type API
Read soil type:
- `readSoilType(type)` reads holding register `0x0020` and maps raw values:
  - `0` mineral
  - `1` sandy
  - `2` clay
  - `3` organic

Write soil type:
- `setSoilType(type)` writes register `0x0020` with function `0x06`

## Epsilon API
`readEpsilon(value)` reads input register `0x0005` using function `0x04` and converts:
- epsilon = `raw / 100.0`

## Coefficient API
`setCompensationCoeff(regAddress, coeffValue)`:
- function `0x06`, writes one raw 16-bit register value

`readCompensationCoeff(regAddress, coeffValue)`:
- function `0x03`, reads one register and converts by `/100.0`

## Address Management
`changeAddress(newAddress)` writes register `0x0200` using:

`[FE] [06] [02] [00] [00] [newAddr] [crc_lo] [crc_hi]`

`scanForAddress(start, end)` probes addresses using the main measurement read command and returns first responsive node (or `0`).

## Minimal Usage
```cpp
RS485Bus bus;
RikaSoilSensor3in1 soil(bus, "soil_1", 0x01);

if (soil.readData()) {
  Serial.print("Soil temp C: ");
  Serial.println(soil.soil_temp);
  Serial.print("Soil VWC %: ");
  Serial.println(soil.soil_vwc);
  Serial.print("Soil EC mS/cm: ");
  Serial.println(soil.soil_ec);
}
```
