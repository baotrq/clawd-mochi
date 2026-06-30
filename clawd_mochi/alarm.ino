// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  ALARM (overlays on top of whatever mode is active)
// ═════════════════════════════════════════════════════════════

void checkAlarm() {
  if (alarmArmed && millis() >= alarmAtMillis) {
    alarmArmed   = false;
    alarmRinging    = true;
    alarmFlashAt    = 0;
    alarmRingStartAt = millis();
  }
}

// Draw a ring label centered on screen, shrinking the text size so a long
// name still fits the 240px width (built-in font: 6px wide * size per char).
void drawRingLabel(const String& label, uint16_t fg) {
  uint8_t size = 4;
  while (size > 1 && (int)(label.length() * 6 * size) > DISP_W - 8) size--;
  int16_t w = label.length() * 6 * size;
  int16_t x = (DISP_W - w) / 2; if (x < 0) x = 0;
  int16_t y = DISP_H / 2 - (8 * size) / 2;
  tft.setTextColor(fg); tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(label);
}

void updateAlarmFlash() {
  if (!alarmRinging) return;
  if (millis() - alarmRingStartAt >= 10000UL) {
    dismissAlarm();
    return;
  }
  if (millis() - alarmFlashAt < 300) return;
  alarmFlashAt = millis();
  alarmFlashOn = !alarmFlashOn;
  if (alarmFlashOn) beep(1800, 150);   // chirp once per flash-on frame
  uint16_t bg = alarmFlashOn ? C_ORANGE : C_WHITE;
  uint16_t fg = alarmFlashOn ? C_WHITE  : C_ORANGE;
  tft.fillScreen(bg);
  drawRingLabel(alarmName.length() ? alarmName : "ALARM!", fg);
}

void dismissAlarm() {
  alarmRinging = false;
  beepStop();
  switchMode(currentMode);  // redraw whatever was on screen before it rang
}

void disarmAlarm() {
  alarmArmed   = false;
  alarmRinging = false;
  beepStop();
}

