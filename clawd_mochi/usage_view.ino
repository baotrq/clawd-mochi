// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  USAGE VIEW (Claude Pro 5h/7d usage, pushed by the web app's 'U' command)
// ═════════════════════════════════════════════════════════════

// Minutes remaining, counted down locally from when the value was received
// (no RTC on this board — same trick as armAlarm/armTimer).
uint16_t usageMinutesRemaining(uint16_t baseMin) {
  uint32_t elapsedMin = (millis() - usageReceivedMillis) / 60000UL;
  if (elapsedMin >= baseMin) return 0;
  return baseMin - elapsedMin;
}

// Outlined progress bar, filled left-to-right by pct (0-100).
void drawUsageBar(int16_t x, int16_t y, int16_t w, int16_t h, int8_t pct) {
  tft.drawRect(x, y, w, h, C_MUTED);
  int16_t innerW = w - 4;
  int16_t fillW = pct <= 0 ? 0 : (int32_t)innerW * pct / 100;
  tft.fillRect(x + 2, y + 2, innerW, h - 4, C_DARKBG);
  if (fillW > 0) tft.fillRect(x + 2, y + 2, fillW, h - 4, C_ORANGE);
}

// Just the "resets in Xh Ym" lines — redrawn on its own every tick so the
// countdown advances without flickering the labels/bars above it.
void drawUsageCountdowns() {
  if (usageSessionPct < 0) return;
  char buf[24];
  tft.setTextColor(C_MUTED); tft.setTextSize(1);

  uint16_t sMin = usageMinutesRemaining(usageSessionResetMin);
  tft.fillRect(12, 100, DISP_W - 24, 10, C_DARKBG);
  tft.setCursor(12, 100);
  snprintf(buf, sizeof(buf), "resets in %dh %dm", sMin / 60, sMin % 60);
  tft.print(buf);

  uint16_t wMin = usageMinutesRemaining(usageWeeklyResetMin);
  tft.fillRect(12, 190, DISP_W - 24, 10, C_DARKBG);
  tft.setCursor(12, 190);
  snprintf(buf, sizeof(buf), "resets in %dh %dm", wMin / 60, wMin % 60);
  tft.print(buf);
}

// Dedicated Usage-mode screen — big % + progress bar + reset countdown for
// both the 5h session window and the 7d weekly window, pushed by the web
// app's 'U' command. Shows a placeholder until the first value arrives.
void drawUsageView() {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(12, 14);
  tft.print("Claude Usage");

  if (usageSessionPct < 0) {
    tft.setTextColor(C_MUTED); tft.setTextSize(1);
    tft.setCursor(12, 70);
    tft.print("waiting for data...");
    return;
  }

  char buf[16];

  // 5h session
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(12, 42);
  tft.print("5h Session");
  tft.setTextColor(C_WHITE); tft.setTextSize(3);
  tft.setCursor(12, 54);
  snprintf(buf, sizeof(buf), "%d%%", usageSessionPct);
  tft.print(buf);
  drawUsageBar(12, 82, DISP_W - 24, 14, usageSessionPct);

  // 7d weekly
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(12, 132);
  tft.print("7d Weekly");
  tft.setTextColor(C_WHITE); tft.setTextSize(3);
  tft.setCursor(12, 144);
  snprintf(buf, sizeof(buf), "%d%%", usageWeeklyPct);
  tft.print(buf);
  drawUsageBar(12, 172, DISP_W - 24, 14, usageWeeklyPct);

  drawUsageCountdowns();
}

// Keep the reset countdowns ticking forward while Usage mode is shown,
// without flickering the bars/labels (same shape as updateClockViewIfShown).
void updateUsageViewIfShown() {
  if (currentMode != MODE_USAGE || usageSessionPct < 0) return;
  static uint16_t lastSessionMin = 0xFFFF;
  static uint16_t lastWeeklyMin  = 0xFFFF;
  uint16_t sMin = usageMinutesRemaining(usageSessionResetMin);
  uint16_t wMin = usageMinutesRemaining(usageWeeklyResetMin);
  if (sMin != lastSessionMin || wMin != lastWeeklyMin) {
    lastSessionMin = sMin;
    lastWeeklyMin  = wMin;
    drawUsageCountdowns();
  }
}

