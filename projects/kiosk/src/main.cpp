#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "utilities.h"   // now comes from projects/kiosk/include/utilities.h

TFT_eSPI tft;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("BOOT: kiosk scaffold");

  // Power on peripherals
  pinMode(BOARD_POWERON, OUTPUT);
  digitalWrite(BOARD_POWERON, HIGH);
  delay(20);

  // Backlight on (simple on/off for now)
  pinMode(BOARD_BL_PIN, OUTPUT);
  digitalWrite(BOARD_BL_PIN, HIGH);

  // Init TFT (uses LilyGO TFT_eSPI config from ../../lib/TFT_eSPI)
  SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
  tft.begin();
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Kiosk scaffold");
  tft.println("Display OK");
}

void loop() {
  delay(1000);
}
