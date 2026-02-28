#include "TDeckKeyboard.h"
#include <Wire.h>
#include "utilities.h"

bool TDeckKeyboard::begin() {
  Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
  pinMode(BOARD_KEYBOARD_INT, INPUT);

  // Give keyboard MCU time to boot after power-on
  delay(150);

  // Try a few gentle probes (non-fatal)
  for (int i = 0; i < 5; i++) {
    Wire.beginTransmission(_addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.println("Keyboard detected on I2C (0x55)");
      _failCount = 0;
      return true;
    }
    delay(50);
  }

  // Not fatal; reads may still work
  Serial.println("Keyboard probe did not ACK (continuing with reads)");
  _failCount = 0;
  return true;
}

int TDeckKeyboard::readKey() {
  int n = Wire.requestFrom((int)_addr, 1);
  if (n == 1 && Wire.available()) {
    _failCount = 0;
    return Wire.read();
  }

  if (++_failCount >= 50) {
    _failCount = 0;
    Wire.beginTransmission(_addr);
    uint8_t err = Wire.endTransmission();
    if (err != 0) Serial.println("Keyboard not responding (0x55)");
  }
  return 0;
}