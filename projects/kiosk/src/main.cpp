#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "utilities.h"      // BOARD_* pin constants
#include "TDeckKeyboard.h"

TDeckKeyboard kb;
TFT_eSPI tft;

void drawInputBox(const String &line) {
  // Redraw a small input area at bottom
  const int x = 10;
  const int y = 200;   // tweak if you want it higher/lower
  const int w = 300;
  const int h = 30;

  tft.fillRect(x, y, w, h, TFT_BLACK);
  tft.drawRect(x, y, w, h, TFT_DARKGREY);
  tft.setCursor(x + 6, y + 8);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.print(line);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("BOOT: kiosk scaffold");

  // Power on peripherals FIRST
  pinMode(BOARD_POWERON, OUTPUT);
  digitalWrite(BOARD_POWERON, HIGH);
  delay(50);

  // Backlight on (simple on/off for now)
  pinMode(BOARD_BL_PIN, OUTPUT);
  digitalWrite(BOARD_BL_PIN, HIGH);

  // Init keyboard after power is on
  bool kbOk = kb.begin();
  Serial.printf("Keyboard init: %s\n", kbOk ? "OK" : "FAIL");

  // Init TFT
  SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
  tft.begin();
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 10);
  tft.println("Kiosk scaffold");
  tft.println("Display OK");

  tft.setCursor(10, 170);
  tft.println("Type below:");

  drawInputBox("");
}

void loop() {
  static String line;
  static bool dirty = false;

  int k = kb.readKey();
  if (k) {
    if (k == 0x08) {                  // Backspace
      if (line.length() > 0) line.remove(line.length() - 1);
      dirty = true;
    } else if (k == 0x0D) {           // Enter
      Serial.printf("SUBMIT: %s\n", line.c_str());
      line = "";
      dirty = true;
    } else if (k >= 32 && k <= 126) { // Printable ASCII
      line += (char)k;
      dirty = true;
    }
  }

  if (dirty) {
    drawInputBox(line);
    dirty = false;
  }

  delay(5);
}