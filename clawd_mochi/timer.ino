// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  TIMER
// ═════════════════════════════════════════════════════════════

void armTimer(uint32_t seconds) {
  timerActive     = true;
  timerRinging    = false;
  timerAtMillis   = millis() + seconds * 1000UL;
  timerDurationMs = seconds * 1000UL;
  if (currentMode == MODE_CLOCK) {
    drawTimerStatic();
    drawTimerTime(seconds / 60, seconds % 60);
  }
}

void disarmTimer() {
  timerActive  = false;
  timerRinging = false;
  beepStop();
}

void checkTimer() {
  if (timerActive && millis() >= timerAtMillis) {
    timerActive      = false;
    timerRinging     = true;
    timerFlashAt     = 0;
    timerRingStartAt = millis();
  }
}

void updateTimerFlash() {
  if (!timerRinging) return;
  if (millis() - timerRingStartAt >= 10000UL) {
    dismissTimer();
    return;
  }
  if (millis() - timerFlashAt < 300) return;
  timerFlashAt = millis();
  timerFlashOn = !timerFlashOn;
  if (timerFlashOn) beep(1500, 150);   // chirp once per flash-on frame
  uint16_t bg = timerFlashOn ? C_ORANGE : C_WHITE;
  uint16_t fg = timerFlashOn ? C_WHITE  : C_ORANGE;
  tft.fillScreen(bg);
  drawRingLabel(timerName.length() ? timerName : "TIMER!", fg);
}

void dismissTimer() {
  timerRinging = false;
  beepStop();
  switchMode(currentMode);
}

void drawTimerStatic() {
  tft.fillScreen(C_DARKBG);
  // Thin accent lines framing the time
  tft.fillRect(16, 86, 208, 1, C_MUTED);
  tft.fillRect(16, 155, 208, 1, C_MUTED);
  // Small label below bottom line
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(105, 165);   // "TIMER" 5*6=30px; (240-30)/2=105
  tft.print("TIMER");
}

void drawTimerTime(uint8_t mm, uint8_t ss) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
  // Clear only the text zone — keeps the static lines and label intact
  // Size 6: "MM:SS" = 5*36=180px wide, 48px tall; x=(240-180)/2=30, y=96
  // Centre of 240×240 is exactly (120,120) — text centre is (30+90, 96+24) = (120,120) ✓
  tft.fillRect(0, 92, DISP_W, 56, C_DARKBG);
  tft.setTextColor(C_WHITE); tft.setTextSize(6);
  tft.setCursor(30, 96);
  tft.print(buf);
}

