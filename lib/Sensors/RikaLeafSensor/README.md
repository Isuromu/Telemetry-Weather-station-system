# RikaLeafSensor Driver Notes

## Purpose
`RikaLeafSensor` is an RS485 Modbus RTU driver for leaf-surface sensors in the RK300-04 / JXBS-3001-YMSD family.

The driver exposes two public values:
- `leaf_temp` in degrees Celsius
- `leaf_humid` in %RH

## Main Read Transaction (`readData`)
The driver sends this request:

`[addr] [03] [00] [00] [00] [02] [crc_lo] [crc_hi]`

- Function `0x03`: read holding registers
- Start register `0x0000`
- Register count `0x0002`

Expected response layout:

`[addr] [03] [04] [hum_hi] [hum_lo] [tmp_hi] [tmp_lo] [crc_lo] [crc_hi]`

Scaling:
- humidity = `rawHum / 10.0`
- temperature = `rawTemp / 10.0` (signed 16-bit)

## Validation and Error Handling
- Driver retries are controlled by `SENSOR_DEFAULT_DRIVER_RETRIES`.
- Transport-level retries/CRC checks are handled by `RS485Bus::SendRequest`.
- Range checks applied in driver:
  - temperature: `-40.0 .. 80.0`
  - humidity: `0.0 .. 100.0`
- On total failure:
  - `setFallbackValues()` sets both values to `-99.0`
  - `markFailure()` updates `SensorDriver` status and error counters

## Address Management
`changeAddress(newAddress)` sends a write-single-register command:

`[FF] [06] [02] [00] [00] [newAddr] [crc_lo] [crc_hi]`

- Register `0x0200` stores node address.
- If command succeeds, `_address` is updated in the driver instance.

`scanForAddress(start, end)` probes each address using the normal read command and returns the first responsive node (or `0` if none found).

## Minimal Usage
```cpp
RS485Bus bus;
RikaLeafSensor leaf(bus, "leaf_1", 0x01);

if (leaf.readData()) {
  Serial.print("Leaf temp C: ");
  Serial.println(leaf.leaf_temp);
  Serial.print("Leaf humidity %: ");
  Serial.println(leaf.leaf_humid);
}
```
