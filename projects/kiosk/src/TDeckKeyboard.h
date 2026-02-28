#pragma once
#include <Arduino.h>

class TDeckKeyboard {
public:
  bool begin();
  int  readKey();         // returns 0 if no key, else 1..255

private:
  uint8_t _addr = 0x55;
  uint8_t _failCount = 0;
};