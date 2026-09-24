#include <Arduino.h>
#include <InvtGD200AProtocol.h>

namespace protocol = invt::gd200a::protocol;

static_assert(protocol::parameterAddress(0x14, 0x00) == 0x1400,
              "P14.00 address encoding changed.");
static_assert(protocol::frequencyHzToRaw(50.0f) == 5000,
              "GD200A frequency scaling changed.");
static_assert(protocol::EXPECTED_DEVICE_CODE == 0x0107,
              "GD200A identification code changed.");

namespace {

void printAddress(const __FlashStringHelper *name, uint16_t address) {
  Serial.print(name);
  Serial.print(F(": 0x"));
  Serial.println(address, HEX);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println(F("[INVT] GD200A staging example"));
  Serial.println(F("[INVT] Read-only placeholder; RS-485 is not initialized."));
  printAddress(F("[INVT] Device code"), protocol::DEVICE_CODE);
  printAddress(F("[INVT] Run state"), protocol::RUN_STATE);
  printAddress(F("[INVT] Fault code"), protocol::FAULT_CODE);
  printAddress(F("[INVT] Operating frequency"),
               protocol::OPERATING_FREQUENCY);
  printAddress(F("[INVT] Setting frequency"), protocol::SETTING_FREQUENCY);
  Serial.println(
      F("[INVT] Answer the questions in examples/InvtGD200A/README.md before live communication."));
}

void loop() { delay(1000); }
