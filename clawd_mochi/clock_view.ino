// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  CLOCK VIEW
// ═════════════════════════════════════════════════════════════

// ═════════════════════════════════════════════════════════════
//  CLOCK
// ═════════════════════════════════════════════════════════════

uint16_t clockNowMinutes() {
  time_t nowSecs;
  time(&nowSecs);
  struct tm timeinfo;
  localtime_r(&nowSecs, &timeinfo);
  return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}

void drawClockView() {
  uint16_t m = clockNowMinutes();
  uint8_t hh = m / 60, mm = m % 60;
  char buf[4];

  uint16_t C_CARD = tft.color565(18, 22, 28);
  uint16_t C_DIM  = tft.color565(35, 33, 30);

  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 3, C_ORANGE);

  // ── Date and Weekday Header ──────────────────
  time_t nowSecs;
  time(&nowSecs);
  struct tm timeinfo;
  localtime_r(&nowSecs, &timeinfo);
  
  static const char* DAY_NAMES[] = {"SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"};
  static const char* MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  
  char dateBuf[32];
  snprintf(dateBuf, sizeof(dateBuf), "%s, %02d %s", DAY_NAMES[timeinfo.tm_wday], timeinfo.tm_mday, MONTH_NAMES[timeinfo.tm_mon]);
  
  tft.setTextColor(C_MUTED); tft.setTextSize(2);
  int16_t dateW = strlen(dateBuf) * 12;
  int16_t dateX = (DISP_W - dateW) / 2;
  if (dateX < 0) dateX = 0;
  tft.setCursor(dateX, 18);
  tft.print(dateBuf);

  // ── Hour card (left) ─────────────────────
  tft.fillRoundRect(8, 42, 108, 84, 8, C_CARD);
  tft.setTextColor(C_WHITE); tft.setTextSize(6);
  snprintf(buf, sizeof(buf), "%02d", hh);
  tft.setCursor(26, 60);
  tft.print(buf);
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(53, 133);
  tft.print("HRS");

  // ── Minute card (right) ──────────────────
  tft.fillRoundRect(124, 42, 108, 84, 8, C_CARD);
  tft.setTextColor(C_WHITE); tft.setTextSize(6);
  snprintf(buf, sizeof(buf), "%02d", mm);
  tft.setCursor(142, 60);
  tft.print(buf);
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(166, 133);
  tft.print("MIN");

  // ── Colon dots ───────────────────────────
  tft.fillRect(118, 72, 6, 6, C_MUTED);
  tft.fillRect(118, 96, 6, 6, C_MUTED);

  // ── Workday dashed bar, red → green (9 AM → 6 PM) ───
  const uint16_t lunchStart = 12 * 60 + 30;
  const uint16_t lunchEnd   = 13 * 60 + 30;
  const uint16_t workStart  = 9  * 60;
  const uint16_t workEnd    = 18 * 60;

  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(8, 162);   tft.print("WORK");
  tft.setCursor(208, 162); tft.print("HOME");

  uint8_t activeDashes = 0;
  if (m >= workStart && m < workEnd)
    activeDashes = (uint32_t)(m - workStart) * 12 / (workEnd - workStart);
  else if (m >= workEnd)
    activeDashes = 12;

  for (uint8_t i = 0; i < 12; i++) {
    tft.fillRect(8 + i * 19, 174, 14, 6, i < activeDashes ? C_ORANGE : C_DIM);
  }

  // ── Single text line: random from message pool ───────────────
  static const char* encMsgs[] = {
    "let's get it!", "keep grinding!", "locked in.",
    "you got this!", "stay focused.", "make it count.",
    "no days off!", "heads down.", "push through!",
    "eyes on the prize", "one step at a time", "almost there!",
    "past halfway!", "keep moving.", "don't stop now!"
  };
  static const char* eggMsgs[] = {
    "go touch grass", "drink water! >_<", "stretch break?",
    "still here huh", "blink twice if ok", "have you eaten?",
    "you look tired...", "vla go brrr", "robot uprising soon",
    "404: chill not found", "skill issue?", "based.",
    "no cap fr fr", "touch some grass bro", "we so back"
  };

  char statusBuf[22];
  if (m < workStart) {
    uint16_t left = workStart - m;
    snprintf(statusBuf, sizeof(statusBuf), "starts in %dh %dm", left / 60, left % 60);
  } else if (m >= workEnd) {
    snprintf(statusBuf, sizeof(statusBuf), "done for today");
  } else if (m >= lunchStart && m < lunchEnd) {
    snprintf(statusBuf, sizeof(statusBuf), "back in %dm", lunchEnd - m);
  } else {
    uint16_t left = workEnd - m;
    snprintf(statusBuf, sizeof(statusBuf), "%dh %dm left", left / 60, left % 60);
  }

  const char* lineText;

  if (m >= lunchStart && m < lunchEnd && random(2) == 0) {
    lineText = "enjoy lunch! :)";
  } else {
    uint8_t roll = random(10);
    if      (roll < 4) { lineText = statusBuf;           }
    else if (roll < 9) { lineText = encMsgs[random(15)]; }
    else               { lineText = eggMsgs[random(15)]; }
  }

  tft.setTextColor(C_MUTED); tft.setTextSize(2);
  tft.setCursor(DISP_W / 2 - strlen(lineText) * 6, 205);
  tft.print(lineText);
}


// Keep Clock mode ticking forward while it's the active mode
void updateClockViewIfShown() {
  if (currentMode != MODE_CLOCK) return;

  if (timerActive) {
    static uint32_t lastTimerSec = 0xFFFF;
    uint32_t remainSec = (timerAtMillis > millis() ? timerAtMillis - millis() : 0) / 1000;
    if (remainSec != lastTimerSec) {
      lastTimerSec = remainSec;
      uint8_t mm = remainSec / 60;
      uint8_t ss = remainSec % 60;
      drawTimerTime(mm, ss);
    }
    return;
  }

  static uint16_t lastMin = 0xFFFF;
  uint16_t m = clockNowMinutes();
  if (m != lastMin) { lastMin = m; drawClockView(); }
}

