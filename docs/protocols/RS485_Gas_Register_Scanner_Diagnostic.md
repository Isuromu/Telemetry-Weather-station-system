# RS485 Gas Register Scanner Diagnostic

Use `examples/JXBS_GasRegisterScanner_Example` when a gas sensor does not answer
the normal driver or when the actual register map differs from the provided
manual.

## What It Does
- Sends Modbus RTU read requests directly through `RS485Bus`.
- Supports function `0x03` holding registers and `0x04` input registers.
- Reads one configured Modbus address.
- Reads an inclusive register range such as `0x0000..0x0016`.
- Sends one read request for the whole configured range.
- Prints every raw word as hex, unsigned, signed, `/10`, `/100`, and `/1000`.
- Detects and prints valid Modbus exception frames when the sensor rejects a
  function or register address.

## How To Configure
Edit `examples/JXBS_GasRegisterScanner_Example/src/config.h`.

Typical first test:

```cpp
#define SCANNER_ADDRESS              0x61
#define SCANNER_FUNCTION_CODE        0x03
#define SCANNER_REGISTER_START       0x0000
#define SCANNER_REGISTER_END         0x0016
```

For the O3/CO/NH3 shield label map, a focused read is:

```cpp
#define SCANNER_ADDRESS              0x61
#define SCANNER_REGISTER_START       0x0006
#define SCANNER_REGISTER_END         0x0008
```

Field result for the tested O3/CO/NH3 shield:
- CO at `0x0006` is confirmed by smoke/burnt-paper testing. Strong smoke
  produced `raw=1250`, which is `125.0 ppm` with `/10` scaling.
- O3 at `0x0007` and NH3 at `0x0008` are accepted from the shield label because
  the CO test confirmed the label map.
- `0x000E` can move with smoke, but treat it as internal/diagnostic data, not as
  the direct CO concentration register.

The range must be no more than 125 registers because that is the Modbus
`0x03`/`0x04` per-request limit.

## Build

```bash
platformio run -e jxbs_gas_register_scanner_example
```

## Output
Each returned register is printed as raw hex, unsigned 16-bit, signed 16-bit,
and quick `/10`, `/100`, `/1000` views. These are display aids only; the scanner
does not assume humidity, temperature, or any gas-specific layout.

Do not bake a register into a driver until the scanner shows a stable CRC-valid
response on the real sensor or the exact sensor/shield label confirms it.
