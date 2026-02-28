#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include "utilities.h"
#include "TDeckKeyboard.h"

TDeckKeyboard kb;
TFT_eSPI tft;

// ---------- UI Layout ----------
static const int UI_SCREEN_W = 320;  // T-Deck is 320x240
static const int UI_SCREEN_H = 240;

static const int UI_PAD = 10;

// Input box
static const int UI_INPUT_X = UI_PAD;
static const int UI_INPUT_Y = 200;
static const int UI_INPUT_W = 300;
static const int UI_INPUT_H = 30;

// Message list area
static const int UI_LIST_X = UI_PAD;
static const int UI_LIST_Y = 40;
static const int UI_LIST_W = 300;
static const int UI_LIST_H = 150;

// Text rendering (built-in font 1 is 6x8, scaled by textSize)
static const int UI_TEXT_SIZE = 2;
static const int UI_CHAR_W = 6 * UI_TEXT_SIZE;
static const int UI_CHAR_H = 8 * UI_TEXT_SIZE;

// ---------- App State ----------
static const int MAX_LINES = 8;
String msgLines[MAX_LINES];
int lineCount = 0;

String input;
int cursorIndex = 0;      // 0..input.length()
bool insertMode = true;   // true=insert, false=overwrite

// ---------- Helpers ----------
static void pushLine(const String &s) {
  if (s.length() == 0) return;

  if (lineCount < MAX_LINES) {
    msgLines[lineCount++] = s;
  } else {
    for (int i = 1; i < MAX_LINES; i++) msgLines[i - 1] = msgLines[i];
    msgLines[MAX_LINES - 1] = s;
  }
}

static void drawHeader() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(UI_TEXT_SIZE);

  tft.setCursor(UI_PAD, 10);
  tft.println("Kiosk scaffold");

  tft.setTextSize(1);
  tft.setCursor(UI_PAD, 28);
  tft.print("Ctrl+T insert=");
  tft.print(insertMode ? "ON" : "OFF");
  tft.print("  Ctrl+B/F move  Ctrl+A/E home/end");
}

static void drawList() {
  tft.fillRect(UI_LIST_X, UI_LIST_Y, UI_LIST_W, UI_LIST_H, TFT_BLACK);
  tft.drawRect(UI_LIST_X, UI_LIST_Y, UI_LIST_W, UI_LIST_H, TFT_DARKGREY);

  tft.setTextSize(UI_TEXT_SIZE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int y = UI_LIST_Y + 6;
  for (int i = 0; i < lineCount; i++) {
    tft.setCursor(UI_LIST_X + 6, y);
    String s = msgLines[i];

    int maxChars = (UI_LIST_W - 12) / UI_CHAR_W;
    if ((int)s.length() > maxChars) {
      s = s.substring(0, maxChars - 3) + "...";
    }
    tft.print(s);

    y += (UI_CHAR_H + 2);
    if (y > (UI_LIST_Y + UI_LIST_H - UI_CHAR_H)) break;
  }
}

static void drawInputBox() {
  tft.fillRect(UI_INPUT_X, UI_INPUT_Y, UI_INPUT_W, UI_INPUT_H, TFT_BLACK);
  tft.drawRect(UI_INPUT_X, UI_INPUT_Y, UI_INPUT_W, UI_INPUT_H, TFT_DARKGREY);

  tft.setTextSize(UI_TEXT_SIZE);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int maxChars = (UI_INPUT_W - 12) / UI_CHAR_W;
  int start = 0;

  if ((int)input.length() > maxChars) {
    if (cursorIndex > maxChars) start = cursorIndex - maxChars;
    if (start < 0) start = 0;
    if (start > (int)input.length()) start = input.length();
  }

  String visible = input.substring(start, min((int)input.length(), start + maxChars));
  tft.setCursor(UI_INPUT_X + 6, UI_INPUT_Y + 8);
  tft.print(visible);

  int cursorCol = cursorIndex - start;
  if (cursorCol < 0) cursorCol = 0;
  if (cursorCol > maxChars) cursorCol = maxChars;

  int cx = (UI_INPUT_X + 6) + (cursorCol * UI_CHAR_W);
  int cy1 = UI_INPUT_Y + 6;
  int cy2 = UI_INPUT_Y + UI_INPUT_H - 6;

  tft.drawLine(cx, cy1, cx, cy2, TFT_CYAN);
}

static void redrawAll() {
  drawHeader();
  drawList();
  drawInputBox();
}

static void handleChar(char c) {
  if (insertMode || cursorIndex >= (int)input.length()) {
    input = input.substring(0, cursorIndex) + c + input.substring(cursorIndex);
  } else {
    input.setCharAt(cursorIndex, c);
  }
  cursorIndex++;
}

// ---------- Setup/Loop ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("BOOT: kiosk scaffold");

  pinMode(BOARD_POWERON, OUTPUT);
  digitalWrite(BOARD_POWERON, HIGH);
  delay(50);

  pinMode(BOARD_BL_PIN, OUTPUT);
  digitalWrite(BOARD_BL_PIN, HIGH);

  bool kbOk = kb.begin();
  Serial.printf("Keyboard init: %s\n", kbOk ? "OK" : "FAIL");

  SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);
  tft.begin();
  tft.setRotation(1);

  redrawAll();
}

void loop() {
  static int last = 0;
  static uint32_t lastMs = 0;

  bool dirtyHeader = false;
  bool dirtyList = false;
  bool dirtyInput = false;

  int k = kb.readKey();
  if (k) {
   Serial.printf("RAW: 0x%02X (%d)\n", k, k);
  }

  if (k) {
    uint32_t now = millis();

    if (k != last || (now - lastMs) > 40) {
      last = k;
      lastMs = now;

      // Cursor / mode controls
      if (k == 0x02) {              // Ctrl+B
        if (cursorIndex > 0) cursorIndex--;
        dirtyInput = true;
      } else if (k == 0x06) {       // Ctrl+F
        if (cursorIndex < (int)input.length()) cursorIndex++;
        dirtyInput = true;
      } else if (k == 0x01) {       // Ctrl+A
        cursorIndex = 0;
        dirtyInput = true;
      } else if (k == 0x05) {       // Ctrl+E
        cursorIndex = input.length();
        dirtyInput = true;
      } else if (k == 0x14) {       // Ctrl+T toggle insert
        insertMode = !insertMode;
        dirtyHeader = true;
        dirtyInput = true;
      }

      // Editing
      else if (k == 0x08) {         // Backspace
        if (cursorIndex > 0 && input.length() > 0) {
          input = input.substring(0, cursorIndex - 1) + input.substring(cursorIndex);
          cursorIndex--;
          dirtyInput = true;
        }
      } else if (k == 0x7F) {       // Delete (if emitted)
        if (cursorIndex < (int)input.length()) {
          input = input.substring(0, cursorIndex) + input.substring(cursorIndex + 1);
          dirtyInput = true;
        }
      } else if (k == 0x0D) {       // Enter
        Serial.printf("SUBMIT: %s\n", input.c_str());
        pushLine(input);
        input = "";
        cursorIndex = 0;
        dirtyList = true;
        dirtyInput = true;
      }

      // Printable ASCII
      else if (k >= 32 && k <= 126) {
        handleChar((char)k);
        dirtyInput = true;
      }
    }
  }

  if (dirtyHeader) drawHeader();
  if (dirtyList) drawList();
  if (dirtyInput) drawInputBox();

  delay(5);
}