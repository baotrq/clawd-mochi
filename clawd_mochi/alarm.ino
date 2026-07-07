// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  ALARM (overlays on top of whatever mode is active)
// ═════════════════════════════════════════════════════════════

void checkAlarm() {
  // 1. Check manual relative countdown alarm
  if (alarmArmed && millis() >= alarmAtMillis) {
    alarmArmed   = false;
    alarmRinging    = true;
    alarmFlashAt    = 0;
    alarmRingStartAt = millis();
    return;
  }

  // 2. Check persistent daily/weekly alarms (requires time sync)
  extern bool timeSynced;
  if (!timeSynced) return;

  time_t nowSecs;
  time(&nowSecs);
  struct tm t;
  localtime_r(&nowSecs, &t);

  static int lastRungMinute = -1;
  static int lastRungHour = -1;
  static int lastRungDay = -1;

  if (t.tm_min == lastRungMinute && t.tm_hour == lastRungHour && t.tm_mday == lastRungDay) {
    return; // Already triggered this minute
  }

  for (int i = 0; i < 10; i++) {
    if (alarmsList[i].enabled) {
      if (alarmsList[i].hour == t.tm_hour && alarmsList[i].minute == t.tm_min) {
        uint8_t dayOfWeek = t.tm_wday; // 0 = Sun, 1 = Mon, ..., 6 = Sat
        if (alarmsList[i].days & (1 << dayOfWeek)) {
          // Match found! Trigger alarm
          alarmRinging = true;
          alarmName = String(alarmsList[i].name);
          alarmFlashAt = 0;
          alarmRingStartAt = millis();
          lastRungMinute = t.tm_min;
          lastRungHour = t.tm_hour;
          lastRungDay = t.tm_mday;
          break;
        }
      }
    }
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

