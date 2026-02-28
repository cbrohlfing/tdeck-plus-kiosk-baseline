#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include "utilities.h"   // uses BOARD_* pin constants in LilyGO repo

TFT_eSPI tft;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("BOOT: kiosk app");

  // Power peripherals
  pinMode(BOARD_POWERON, OUTPUT);
  digitalWrite(BOARD_POWERON, HIGH);
  delay(20);

  // Backlight
  pinMode(BOARD_BL_PIN, OUTPUT);
  digitalWrite(BOARD_BL_PIN, HIGH);

  // SPI and TFT
  SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
  tft.begin();
  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Kiosk app scaffold");
  tft.println("Keyboard next...");
}

void loop() {
  delay(1000);
}