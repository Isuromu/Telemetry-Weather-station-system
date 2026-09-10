#include <Arduino.h>
#include <CommandProcessor.h>
#include <ProjectConfig.h>

namespace {

HardwareSerial vfdSerial(2);
PrintController logger(Serial, true);
RS485Bus rs485;
DelixiCDIE100 vfd(rs485, logger, irrigation::ActiveInverter);
PumpController pump(vfd, logger, irrigation::ActiveMotor);
CommandProcessor commands(pump, vfd, logger);
SerialCommandSource serialCommands(commands, logger);

uint32_t arduinoSerialFrame(irrigation::SerialFrame frame) {
  switch (frame) {
    case irrigation::SerialFrame::EightN2:
      return SERIAL_8N2;
    case irrigation::SerialFrame::EightE1:
      return SERIAL_8E1;
    case irrigation::SerialFrame::EightO1:
      return SERIAL_8O1;
    case irrigation::SerialFrame::EightN1:
    default:
      return SERIAL_8N1;
  }
}

void printBootProfile() {
  logger.println(F("[SYSTEM] IrrigationAutomationFirmware"), true);
  logger.print(F("[BOARD] "), true);
  logger.println(irrigation::ActiveBoard.displayName, true);
  logger.print(F("[RS485] RX=GPIO"), true);
  logger.print(irrigation::ActiveBoard.rs485RxPin, true);
  logger.print(F(", TX=GPIO"), true);
  logger.print(irrigation::ActiveBoard.rs485TxPin, true);
  logger.println(F(", automatic direction"), true);
  logger.print(F("[VFD] "), true);
  logger.println(irrigation::ActiveInverter.model, true);
  logger.print(F("[MOTOR] "), true);
  logger.print(irrigation::ActiveMotor.manufacturer, true);
  logger.print(F(" "), true);
  logger.println(irrigation::ActiveMotor.model, true);
  logger.println(
      F("[SYSTEM] GPIO2 is reserved; it is not driven by this pump example."),
      true);
}

}  // namespace

void setup() {
  Serial.begin(irrigation::ActiveBoard.debugBaud);
  delay(300);
  printBootProfile();

  const Rs485DirectionMode directionMode =
      irrigation::ActiveBoard.rs485DirectionMode ==
              irrigation::Rs485DirectionMode::Automatic
          ? Rs485DirectionMode::Automatic
          : Rs485DirectionMode::Manual;
  rs485.setDirectionMode(directionMode,
                         irrigation::ActiveBoard.rs485DirectionPin,
                         irrigation::ActiveBoard.rs485DirectionActiveHigh);
  rs485.setDebug(&logger);
  rs485.setTimings(0, 0, 5);
  rs485.begin(vfdSerial, irrigation::ActiveInverter.modbusBaud,
              irrigation::ActiveBoard.rs485RxPin,
              irrigation::ActiveBoard.rs485TxPin,
              arduinoSerialFrame(irrigation::ActiveInverter.modbusFrame));

  vfd.setDebug(DEBUG_MODBUS != 0);
  vfd.begin();
  pump.begin();

  if (vfd.ping()) {
    // Boot policy: issue only a stop command; never send a run command.
    pump.stop();
    const DelixiConfigurationReport report =
        vfd.checkConfiguration(irrigation::ActiveMotor);
    pump.setConfigurationValid(report.valid());
  } else {
    pump.setConfigurationValid(false);
  }

  logger.println(F("[SYSTEM] Boot complete. Pump start is never automatic."),
                 true);
  logger.println(F("[SYSTEM] Type: help"), true);
}

void loop() {
  serialCommands.poll(Serial);
  pump.poll();
  delay(1);
}


