// ChirpStack v4 JavaScript codec, klapan payload v1, FPort 10.
function decodeUplink(input) {
  var b = input.bytes;
  if (input.fPort !== 10) return {errors: ["Expected FPort 10"]};
  if (!b || b.length !== 12 || b[0] !== 1) return {errors: ["Invalid payload length/version"]};
  function u16(i) { return (b[i] << 8) | b[i + 1]; }
  function i16(i) { var v = u16(i); return v >= 32768 ? v - 65536 : v; }
  var beforeValid = (b[1] & 2) !== 0;
  var afterValid = (b[1] & 4) !== 0;
  var valveOpen = (b[1] & 1) !== 0;
  var batteryVoltage = u16(2) / 1000;
  var pressureBefore = beforeValid ? i16(4) / 1000 : null;
  var pressureAfter = afterValid ? i16(6) / 1000 : null;
  var temperatureBefore = beforeValid ? i16(8) / 100 : null;
  var temperatureAfter = afterValid ? i16(10) / 100 : null;
  return {data: {
    version: b[0],
    valve_commanded_open: valveOpen,
    valve_status: valveOpen ? "OPEN" : "CLOSED",
    battery_v: batteryVoltage,
    battery_voltage: batteryVoltage.toFixed(3) + " V",
    before_valid: beforeValid,
    after_valid: afterValid,
    before_sensor_status: beforeValid ? "OK" : "NOT_AVAILABLE",
    after_sensor_status: afterValid ? "OK" : "NOT_AVAILABLE",
    pressure_before_bar: pressureBefore,
    pressure_after_bar: pressureAfter,
    temperature_before_c: temperatureBefore,
    temperature_after_c: temperatureAfter,
    pressure_difference_bar: pressureBefore !== null && pressureAfter !== null ? pressureBefore - pressureAfter : null
  }};
}

function encodeDownlink(input) {
  var command = input.data;
  if (command && typeof command === "object") command = command.command;
  if (typeof command === "string") command = command.trim().toLowerCase();
  if (command !== "open" && command !== "close" && command !== "status") {
    return {errors: ["command must be open, close or status"]};
  }
  var bytes = [];
  for (var i = 0; i < command.length; i++) bytes.push(command.charCodeAt(i));
  return {fPort: 10, bytes: bytes};
}