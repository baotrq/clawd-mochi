// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  NUMERIC INPUT PROMPT ("t" set-clock, "r" set-alarm — Clock mode only)
// ═════════════════════════════════════════════════════════════

void drawInputPrompt() {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(12, 30);
  if (inputKind == INPUT_CLOCK_SET) {
    tft.print("Set time HHMM:");
  } else if (inputKind == INPUT_ALARM_SET) {
    tft.print("Alarm at HHMM:");
  } else if (inputKind == INPUT_TIMER_SEC) {
    tft.print("Timer in (sec):");
  } else if (inputKind == INPUT_POMO_WORK) {
    tft.print("Focus time (min):");
  } else if (inputKind == INPUT_POMO_BREAK) {
    tft.print("Break time (min):");
  }
  tft.fillRect(12, 70, DISP_W - 24, 30, C_DARKBG);
  tft.setTextColor(C_GREEN); tft.setTextSize(3);
  tft.setCursor(12, 70);
  tft.print(inputBuf);
  tft.print("_");
  // Alarm/timer accept an optional name after a space (e.g. "0730 Wake up")
  if (inputKind == INPUT_ALARM_SET || inputKind == INPUT_TIMER_SEC) {
    tft.setTextColor(C_MUTED); tft.setTextSize(1);
    tft.setCursor(12, 120);
    tft.print("space then name = label");
  }
}

void enterTimeInput(InputKind kind) {
  inputKind = kind;
  inputBuf  = "";
  drawInputPrompt();
}

void cancelInput() {
  inputKind = INPUT_NONE;
  inputBuf  = "";
  switchMode(currentMode);  // redraw whatever mode is underneath (Clock)
}

void armAlarm(uint16_t minutesFromNow) {
  alarmArmed   = true;
  alarmAtMillis = millis() + (uint32_t)minutesFromNow * 60000UL;
}

void submitInput() {
  if (inputBuf.length() == 0) { cancelInput(); return; }
  if (inputKind == INPUT_CLOCK_SET) {
    int val = inputBuf.toInt();
    uint8_t hh = (val / 100) % 24;
    uint8_t mm = (val % 100) > 59 ? 59 : (val % 100);
    
    time_t nowSecs;
    time(&nowSecs);
    struct tm t;
    localtime_r(&nowSecs, &t);
    t.tm_hour = hh;
    t.tm_min = mm;
    t.tm_sec = 0;
    t.tm_isdst = -1;
    time_t epoch = mktime(&t);
    
    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    timeSynced = true;
    Serial.print("Clock set to "); Serial.println(inputBuf);
  } else if (inputKind == INPUT_ALARM_SET) {
    int sp = inputBuf.indexOf(' ');
    String numPart = sp >= 0 ? inputBuf.substring(0, sp) : inputBuf;
    alarmName = sp >= 0 ? inputBuf.substring(sp + 1) : "";
    alarmName.trim();
    if (alarmName.length() > 18) alarmName = alarmName.substring(0, 18);
    int val = numPart.toInt();
    uint8_t hh = (val / 100) % 24;
    uint8_t mm = (val % 100) > 59 ? 59 : (val % 100);
    uint16_t alarmTargetMin = hh * 60 + mm;
    uint16_t nowMin = clockNowMinutes();
    int16_t diffMin = alarmTargetMin - nowMin;
    if (diffMin <= 0) {
      diffMin += 1440; // target is earlier today or now; set for tomorrow
    }
    armAlarm(diffMin);
    Serial.print("Alarm set for ");
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hh, mm);
    Serial.print(timeBuf);
    Serial.print(" (in "); Serial.print(diffMin); Serial.println(" min)");
    if (alarmName.length()) { Serial.print("Alarm name: "); Serial.println(alarmName); }
  } else if (inputKind == INPUT_TIMER_SEC) {
    int sp = inputBuf.indexOf(' ');
    String numPart = sp >= 0 ? inputBuf.substring(0, sp) : inputBuf;
    timerName = sp >= 0 ? inputBuf.substring(sp + 1) : "";
    timerName.trim();
    if (timerName.length() > 18) timerName = timerName.substring(0, 18);
    uint32_t secs = numPart.toInt();
    if (secs > 0) {
      armTimer(secs);
      Serial.print("Timer set to "); Serial.print(secs); Serial.println(" seconds");
      if (timerName.length()) { Serial.print("Timer name: "); Serial.println(timerName); }
    }
  } else if (inputKind == INPUT_POMO_WORK) {
    uint32_t mins = inputBuf.toInt();
    if (mins > 0) {
      pomoWorkMs = mins * 60000UL;
      Serial.print("Pomo work set to "); Serial.print(mins); Serial.println(" min");
    }
  } else if (inputKind == INPUT_POMO_BREAK) {
    uint32_t mins = inputBuf.toInt();
    if (mins > 0) {
      pomoBreakMs = mins * 60000UL;
      Serial.print("Pomo break set to "); Serial.print(mins); Serial.println(" min");
    }
  }
  cancelInput();
}

void handleInputChar(char c) {
  // Alarm/timer may carry an optional name after a single space; clock/pomo
  // stay digits-only as before.
  bool nameable = (inputKind == INPUT_ALARM_SET || inputKind == INPUT_TIMER_SEC);
  bool hasSpace = inputBuf.indexOf(' ') >= 0;
  if (c == '\n' || c == '\r') {
    submitInput();
  } else if (c == '\b' || c == 127) {
    if (inputBuf.length() > 0) { inputBuf.remove(inputBuf.length() - 1); drawInputPrompt(); }
  } else if (c >= '0' && c <= '9' && !hasSpace && inputBuf.length() < 4) {
    // digits form the time/duration (the part before the optional space)
    inputBuf += c;
    drawInputPrompt();
  } else if (nameable && c == ' ' && !hasSpace && inputBuf.length() > 0) {
    // one space begins the name (needs a number first)
    inputBuf += c;
    drawInputPrompt();
  } else if (nameable && hasSpace && c >= 32 && c <= 126 && inputBuf.length() < 24) {
    // past the space: collect printable name chars (further spaces allowed)
    inputBuf += c;
    drawInputPrompt();
  }
}

