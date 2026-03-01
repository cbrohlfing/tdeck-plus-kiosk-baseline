#include <Arduino.h>
#include <TFT_eSPI.h>

#include "utilities.h"
#include "TDeckKeyboard.h"

static TFT_eSPI tft;
static TDeckKeyboard kb;

// -------------------- Layout --------------------
static const int HEADER_Y      = 2;
static const int STATUS_Y      = 18;
static const int MSG_TOP_Y     = 36;
static const int INPUT_TOP_Y   = 210;
static const int INPUT_TEXT_Y  = INPUT_TOP_Y + 6;
static const int LINE_H        = 16;
static const int LEFT_X        = 6;

static const int MAX_LINES = 30;   // bump so scrolling is real
static String lines[MAX_LINES];
static int lineCount = 0;

static String input;
static int cursorPos = 0;

static uint32_t lastBlink = 0;
static bool cursorOn = true;

// -------------------- Trackball pins --------------------
static const uint8_t PIN_RIGHT = BOARD_TBOX_G02;
static const uint8_t PIN_UP    = BOARD_TBOX_G01;
static const uint8_t PIN_LEFT  = BOARD_TBOX_G04;
static const uint8_t PIN_DOWN  = BOARD_TBOX_G03;
static const uint8_t PIN_CLICK = BOARD_BOOT_PIN;

// edge detect
static bool lastBtnRight = true;
static bool lastBtnLeft  = true;
static bool lastBtnUp    = true;
static bool lastBtnDown  = true;
static bool lastBtnClick = true;

static uint32_t lastTbMoveMs = 0;
static const uint32_t TB_DEBOUNCE_MS = 25;

// -------------------- Mode + Scroll --------------------
enum class TBMode { Caret, Scroll };
static TBMode tbMode = TBMode::Caret;

// scrollOffset: 0 = newest (bottom); increases = older
static int scrollOffset = 0;

// -------------------- Helpers --------------------
static int maxVisibleLines() {
  return (INPUT_TOP_Y - MSG_TOP_Y) / LINE_H;
}

static void clampScrollOffset() {
  int maxVis = maxVisibleLines();
  if (maxVis <= 0) { scrollOffset = 0; return; }

  int maxOffset = lineCount - maxVis;
  if (maxOffset < 0) maxOffset = 0;

  if (scrollOffset < 0) scrollOffset = 0;
  if (scrollOffset > maxOffset) scrollOffset = maxOffset;
}

static bool pressedEdge(uint8_t pin, bool &lastState) {
  bool now = digitalRead(pin); // INPUT_PULLUP: released=HIGH, pressed=LOW
  bool pressed = (lastState == HIGH && now == LOW);
  lastState = now;
  return pressed;
}

// -------------------- Drawing --------------------
static void drawHeader() {
  tft.fillRect(0, 0, tft.width(), MSG_TOP_Y - 1, TFT_BLACK);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Kiosk Milestone 2: Trackball + Keyboard", LEFT_X, HEADER_Y, 2);

  tft.drawFastHLine(0, INPUT_TOP_Y, tft.width(), TFT_DARKGREY);
}

static void drawStatus() {
  tft.fillRect(0, STATUS_Y, tft.width(), 16, TFT_BLACK);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  String modeStr = (tbMode == TBMode::Caret) ? "MODE: CARET" : "MODE: SCROLL";
  String s = modeStr +
             "  lines=" + String(lineCount) +
             "  off=" + String(scrollOffset) +
             "  cur=" + String(cursorPos);

  tft.drawString(s, LEFT_X, STATUS_Y, 2);
}

static void clearMessageArea() {
  tft.fillRect(0, MSG_TOP_Y, tft.width(), INPUT_TOP_Y - MSG_TOP_Y - 1, TFT_BLACK);
}

static void drawMessages() {
  clearMessageArea();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  int maxVis = maxVisibleLines();
  if (maxVis <= 0) return;

  clampScrollOffset();

  int start = lineCount - maxVis - scrollOffset;
  if (start < 0) start = 0;

  int end = start + maxVis;
  if (end > lineCount) end = lineCount;

  int y = MSG_TOP_Y;
  for (int i = start; i < end; i++) {
    tft.drawString(lines[i], LEFT_X, y, 2);
    y += LINE_H;
  }

  drawStatus();
}

static String renderInputWithCaret() {
  if (cursorPos < 0) cursorPos = 0;
  if (cursorPos > (int)input.length()) cursorPos = input.length();

  String left = input.substring(0, cursorPos);
  String right = input.substring(cursorPos);

  char caretChar = cursorOn ? '|' : ' ';
  return String("> ") + left + caretChar + right;
}

static void drawInputBar() {
  tft.fillRect(0, INPUT_TOP_Y + 1, tft.width(), tft.height() - (INPUT_TOP_Y + 1), TFT_BLACK);
  tft.drawFastHLine(0, INPUT_TOP_Y, tft.width(), TFT_DARKGREY);

  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(renderInputWithCaret(), LEFT_X, INPUT_TEXT_Y, 2);

  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  if (tbMode == TBMode::Caret) {
    tft.drawString("TB: L/R caret | U/D scroll | Click=toggle mode", LEFT_X, INPUT_TEXT_Y + 16, 2);
  } else {
    tft.drawString("TB: U/D scroll | L/R page | Click=toggle mode", LEFT_X, INPUT_TEXT_Y + 16, 2);
  }

  drawStatus();
}

// -------------------- Message buffer --------------------
static void pushLine(const String& s) {
  if (s.length() == 0) return;

  if (lineCount < MAX_LINES) {
    lines[lineCount++] = s;
  } else {
    for (int i = 1; i < MAX_LINES; i++) lines[i - 1] = lines[i];
    lines[MAX_LINES - 1] = s;
  }

  // when new content is added, snap to newest
  scrollOffset = 0;
}

// -------------------- Keyboard input --------------------
static void insertCharAtCursor(char c) {
  if (cursorPos < 0) cursorPos = 0;
  if (cursorPos > (int)input.length()) cursorPos = input.length();

  input = input.substring(0, cursorPos) + c + input.substring(cursorPos);
  cursorPos++;
}

static void backspaceAtCursor() {
  if (cursorPos <= 0 || input.length() == 0) return;
  input.remove(cursorPos - 1, 1);
  cursorPos--;
}

static void submitLine() {
  pushLine(input);
  input = "";
  cursorPos = 0;
  drawMessages();
  drawInputBar();
}

static void applyKey(int k) {
  if (k == 0) return;

  if (k == 0x08) { // backspace
    backspaceAtCursor();
    drawInputBar();
    return;
  }

  if (k == 0x0D) { // enter
    submitLine();
    return;
  }

  if (k >= 32 && k <= 126) {
    insertCharAtCursor((char)k);
    drawInputBar();
  }
}

// -------------------- Trackball handling --------------------
static void toggleMode() {
  tbMode = (tbMode == TBMode::Caret) ? TBMode::Scroll : TBMode::Caret;
  Serial.printf("TB: CLICK -> toggle mode: %s\n", tbMode == TBMode::Caret ? "CARET" : "SCROLL");
  drawInputBar();
}

static void handleTrackball() {
  uint32_t nowMs = millis();

  if (pressedEdge(PIN_CLICK, lastBtnClick)) {
    toggleMode();
    return;
  }

  if (nowMs - lastTbMoveMs < TB_DEBOUNCE_MS) return;

  bool changed = false;

  // UP/DOWN always scroll messages (visible), regardless of mode
  if (pressedEdge(PIN_UP, lastBtnUp)) {
    scrollOffset++;
    clampScrollOffset();
    Serial.printf("TB: UP -> scrollOffset=%d\n", scrollOffset);
    drawMessages();
    changed = true;
  }

  if (pressedEdge(PIN_DOWN, lastBtnDown)) {
    scrollOffset--;
    clampScrollOffset();
    Serial.printf("TB: DOWN -> scrollOffset=%d\n", scrollOffset);
    drawMessages();
    changed = true;
  }

  // LEFT/RIGHT depends on mode
  if (pressedEdge(PIN_LEFT, lastBtnLeft)) {
    if (tbMode == TBMode::Caret) {
      if (cursorPos > 0) cursorPos--;
      Serial.printf("TB: LEFT -> caret=%d\n", cursorPos);
      drawInputBar();
    } else {
      scrollOffset += max(1, maxVisibleLines() / 2);
      clampScrollOffset();
      Serial.printf("TB: LEFT -> page up scrollOffset=%d\n", scrollOffset);
      drawMessages();
    }
    changed = true;
  }

  if (pressedEdge(PIN_RIGHT, lastBtnRight)) {
    if (tbMode == TBMode::Caret) {
      if (cursorPos < (int)input.length()) cursorPos++;
      Serial.printf("TB: RIGHT -> caret=%d\n", cursorPos);
      drawInputBar();
    } else {
      scrollOffset -= max(1, maxVisibleLines() / 2);
      clampScrollOffset();
      Serial.printf("TB: RIGHT -> page down scrollOffset=%d\n", scrollOffset);
      drawMessages();
    }
    changed = true;
  }

  if (changed) lastTbMoveMs = nowMs;
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial.println("BOOT: kiosk milestone 2 (VISIBLE scroll + caret)");

  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_UP,    INPUT_PULLUP);
  pinMode(PIN_LEFT,  INPUT_PULLUP);
  pinMode(PIN_DOWN,  INPUT_PULLUP);
  pinMode(PIN_CLICK, INPUT_PULLUP);

  pinMode(BOARD_POWERON, OUTPUT);
  digitalWrite(BOARD_POWERON, HIGH);
  delay(50);

  pinMode(BOARD_BL_PIN, OUTPUT);
  digitalWrite(BOARD_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true);

  drawHeader();

  bool ok = kb.begin();
  Serial.println(ok ? "Keyboard init: OK" : "Keyboard init: FAIL");

  // Seed enough content to make scrolling obvious
  pushLine("Welcome!");
  pushLine("This build makes scroll/caret very obvious.");
  for (int i = 1; i <= 20; i++) {
    pushLine("Seed line " + String(i) + " (scroll test)");
  }
  pushLine("Type below. Enter submits a new line.");

  drawMessages();
  drawInputBar();
}

void loop() {
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    cursorOn = !cursorOn;
    drawInputBar();
  }

  handleTrackball();

  int k = kb.readKey();
  if (k) {
    if (k == 0x0D) Serial.println("Key: ENTER");
    else if (k == 0x08) Serial.println("Key: BACKSPACE");
    else if (k >= 32 && k <= 126) Serial.printf("Key: 0x%02X (%d) '%c'\n", k, k, (char)k);
    else Serial.printf("Key: 0x%02X (%d)\n", k, k);

    applyKey(k);
  }

  delay(5);
}