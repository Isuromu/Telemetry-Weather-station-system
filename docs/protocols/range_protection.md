# Range protection & response sanity checks (all sensors)

Modbus CRC/prefix/length validation proves you received a **valid frame**, not that the **values make sense**. Range protection is the second line of defense that catches mis-parsing and many sensor-fault states while keeping drivers stable on noisy RS485 buses.

## What range protection should do
- **Hard bounds**: Reject samples that are physically impossible for that sensor model (manufacturer measurement range).
- **Soft plausibility** (optional): Flag values outside typical real-world conditions (useful for alerts, not necessarily rejection).
- **Rate-of-change (ROC)**: Reject or flag jumps that are impossible over one poll interval (often indicates misalignment or wrong scaling).
- **Fault patterns**: Treat repeating raw placeholders like `0xFFFF`, `0x7FFF`, `0x8000` as faults if they persist for that sensor.

## Recommended policy (driver-level)
1) Extract frame from raw RX buffer using **prefix + expected length + CRC16**.
2) Decode raw words using the datasheet’s **signedness + endianness + scaling**.
3) Apply hard bounds per field. If any fail: **discard sample**, increment `invalid_count`, keep `last_good`.
4) If `invalid_count` reaches a threshold (e.g. 3–10): raise a `sensor_fault` flag.

## Global range table (no omissions)
Each row references the corresponding protocol file. Values marked **UNCERTAIN** mean the provided manual did not expose numeric limits; you must confirm via vendor tool or by capturing real frames.

| Protocol file | Hard bounds (engineering units) | Notes |
|---|---|---|
| `485_Instrument_Shelter_Temp_Humi_Luxmeter_RS485_output.md` | Temperature: -40..80 °C; Humidity: 0..100 %RH; Illuminance: 0..200000 Lux | Temperature: Use family-default clamp unless shelter manual specifies otherwise |
| `Basic_Assembly_Accessories_for_Shutter_Box_non_sensor_item.md` | N/A | Not a measurement sensor/item. |
| `JXBS_3001_CO_Carbon_Monoxide_Sensor_RS485.md` | CO: 0..2000 ppm | CO: manual lists 0–1000/2000 ppm variants; driver should configure expected max |
| `JXBS_3001_NH3_Ammonia_Sensor_RS485.md` | NH3: 0..5000 ppm | NH3: manual lists 0–100/1000/5000 ppm variants; driver should configure expected max |
| `JXBS_3001_NO2_Nitrogen_Dioxide_Sensor_RS485_variant_required_manual_provided_is_analog.md` | NO2: 0..2000 ppm | NO2: manual lists 0–20 ppm or 0–2000 ppm variants (analog doc); configure expected max for your RS485 model |
| `JXBS_3001_NPK_RS_Soil_Multi_parameter_Sensor_RS485.md` | Soil moisture: 0..100 %; Soil temperature: -40..80 °C; Soil EC: 0..10000 µS/cm; Soil pH: 3..9 pH; N/P/K: 0..UNCERTAIN mg/kg | Soil moisture: raw u16 in 0.1% → 0..1000; Soil temperature: raw s16 in 0.1°C → -400..800; Soil EC: raw u16; Soil pH: raw u16 in 0.01 pH → 300..900; N/P/K: doc excerpt didn’t include explicit max; treat absurd values (e.g. > 5000) as suspicious |
| `JXBS_3001_O3_Ozone_Sensor_RS485.md` | O3: 0..100 ppm | O3: manual lists 0–10 ppm or 0–100 ppm variants; driver should configure expected max; O3: Any value > configured max+10% is a fault |
| `JXBS_3001_PH_RS_Water_pH_Sensor_RS485_Modbus.md` | pH: 0..14 pH; Water temperature: UNCERTAIN..UNCERTAIN °C | pH: raw u16 in 0.01 pH → 0..1400; Water temperature: manual text extraction didn’t expose numeric limits; use a conservative clamp -10..60°C unless your process needs wider |
| `JXBS_3001_PM2_5_PM10_Sensor_Shutter_Box_RS485.md` | PM2.5: 0..300 µg/m³; PM10: 0..300 µg/m³ | PM2.5: raw u16 1 µg/m³; PM10: raw u16 1 µg/m³ |
| `JXBS_3001_TR_Soil_Comprehensive_Sensor_RS485.md` | Soil moisture: 0..100 %; Soil temperature: -40..80 °C; Soil EC: 0..10000 µS/cm; Soil pH: 3..9 pH; N/P/K: 0..UNCERTAIN mg/kg | Soil moisture: raw u16 in 0.1%; Soil temperature: raw s16 in 0.1°C; Soil EC: raw u16; Soil pH: raw u16 in 0.01 pH; N/P/K: explicit max not extracted; apply plausibility filter |
| `JXBS_3001_YMSD_Leaf_Surface_Humidity_Transmitter_RS485.md` | Leaf humidity: 0..100 %RH; Leaf temperature: -20..80 °C | Leaf humidity: raw u16 in 0.1%RH → expect 0..1000; Leaf temperature: raw s16 in 0.1°C → expect -200..800 |
| `JXEC_T_Series_Conductivity_Probe_Water_EC_TDS_Temp_RS485.md` | EC/TDS/Temp: UNCERTAIN..UNCERTAIN | EC/TDS/Temp: numeric measurement ranges not extractable from provided doc text; require manual table or vendor tool readouts |
| `JXSZ_1001_SS_Water_Suspended_Solids_Sensor_RS485.md` | Suspended solids: 0..20000 mg/L; Water temperature (if provided): 0..50 °C | Suspended solids: raw u16? (per manual: range 0–20000 mg/L); Water temperature (if provided): manual lists operating temp 0–50°C |
| `LoRa_Gateway_with_Ethernet_Output.md` | N/A | Not a measurement sensor/item. |
| `LoRa_Module_radio_module_station_accessory.md` | N/A | Not a measurement sensor/item. |
| `PJ2_COM_TTL_RS485_Converter_Module_transport_accessory.md` | N/A | Not a measurement sensor/item. |
| `RS485_Atmospheric_Pressure_Sensor_manual_appears_to_include_T_H_pressure.md` | Atmospheric pressure: 10..1200 mbar | Atmospheric pressure: Typical ambient is ~800..1100 mbar; treat outside that as suspicious but not invalid |
| `RS485_Evaporation_Sensor_JXCT_family.md` | Evaporation (height): 0..200 mm | Evaporation (height): resolution ≤0.01 mm; Evaporation (height): Use a conservative ROC clamp, e.g. > 50 mm/hour is very likely a fault |
| `RS485_Illuminance_Sensor.md` | Illuminance: 0..200000 Lux | Illuminance: raw u32 (two regs). Some variants list 0..65535; accept up to 200000 per spec, treat > 300000 as suspicious |
| `RS485_Optical_Rain_Sensor.md` | Rainfall (cumulative or period per doc): 0..UNCERTAIN mm | Rainfall (cumulative or period per doc): resolution 0.1 mm; Rainfall (cumulative or period per doc): Max instantaneous rainfall is 0.4 mm/s → max increment = 0.4*Δt seconds |
| `RS485_Photosynthetically_Active_Radiation_PAR_Sensor.md` | PAR (as reported by sensor): 0..2000 W/m² | PAR (as reported by sensor): raw u16, resolution 1 W/m² |
| `RS485_TVOC_Sensor_multi_parameter_air_quality_board.md` | TVOC: 0..1000 ppb; CO2: 400..5000 ppm; Humidity (if used): 0..100 %RH; Temperature (if used): -40..80 °C | TVOC: doc range is 0–1000 ppb; if driver reports ppm, clamp to 0..1.0 ppm; CO2: doc range 400–5000 ppm; Humidity (if used): 0..100%RH; Temperature (if used): -40..80°C typical for this family |
| `RS485_Temperature_Humidity_Sensor_JXCT_family.md` | Humidity: 0..100 %RH; Temperature: -40..80 °C | Humidity: raw u16 in 0.1%RH → 0..1000; Temperature: raw s16 in 0.1°C → -400..800 |
| `RS485_Total_Solar_Radiation_Sensor.md` | Total solar radiation: 0..1500 W/m² | Total solar radiation: raw u16, resolution 1 W/m² |
| `RS485_UV_Rays_Sensor.md` | UV intensity: 0..150 W/m²; Humidity (if read): 0..100 %RH; Temperature (if read): -40..80 °C | UV intensity: raw u16 in 0.1 W/m² → 0..1500; Humidity (if read): raw u16 0.1%RH → 0..1000; Temperature (if read): raw s16 0.1°C → -400..800 |
| `RS485_Wind_Direction_Sensor.md` | Wind direction: 0..360 deg | Wind direction: raw u16 degrees |
| `RS485_Wind_Speed_Sensor.md` | Wind speed: 0..30 m/s | Wind speed: raw u16 in 0.1m/s → 0..300; Wind speed: Reject single-sample jumps > 15 m/s unless verified by multiple consecutive reads |
| `SO2_Gas_Sensor_RS485_variant_required_manual_provided_is_analog.md` | SO2: 0..2000 ppm | SO2: manual lists 0–20 ppm or 0–2000 ppm variants (analog doc); configure expected max for your RS485 model |
| `Soil_7_in_1_Sensor_LoRa_variant.md` | Soil params: UNCERTAIN..UNCERTAIN | Soil params: LoRa transport; apply the same physical bounds as the RS485 soil sensor model you actually use |
| `Water_EC_Sensor_RS485_project_item_maps_to_conductivity_probe_doc.md` | Water EC/TDS/Temp: UNCERTAIN..UNCERTAIN | Water EC/TDS/Temp: measurement ranges depend on probe model; require vendor spec table |