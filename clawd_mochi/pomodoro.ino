// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  POMODORO TIMER (ticks in the background regardless of currentMode;
//  only draws while Pomodoro mode is the active mode)
// ═════════════════════════════════════════════════════════════

void drawPomodoroIdle() {
  // Reset animation; updatePomodoroIdleAnim() handles all drawing
  pomoIdlePhase  = 0;
  pomoIdleStep   = 0;
  pomoIdleNextAt = 0;
}

// Pixel art tomato: 0=skip, 1=red, 2=highlight, 3=leaf, 4=stem
static const uint8_t TOM_PIX[8][8] = {
  {0,3,3,3,0,0,0,0},
  {0,0,4,0,0,0,0,0},
  {0,1,1,1,1,1,0,0},
  {1,1,2,1,1,1,1,0},
  {1,1,1,1,1,1,1,0},
  {1,1,1,1,1,1,1,0},
  {0,1,1,1,1,1,0,0},
  {0,0,1,1,1,0,0,0},
};

void drawTomPixel(int16_t ox, int16_t oy, uint8_t B) {
  uint16_t RED  = tft.color565(210, 38, 18);
  uint16_t RED2 = tft.color565(255, 110, 70);
  uint16_t GRN  = tft.color565(55, 175, 25);
  uint16_t SGRN = tft.color565(28, 115, 12);
  for (uint8_t r = 0; r < 8; r++) {
    for (uint8_t c = 0; c < 8; c++) {
      uint8_t v = TOM_PIX[r][c];
      if (!v) continue;
      uint16_t col = (v==1)?RED:(v==2)?RED2:(v==3)?GRN:SGRN;
      tft.fillRect(ox + c*B, oy + r*B, B, B, col);
    }
  }
}

void updatePomodoroIdleAnim() {
  if (currentMode != MODE_POMODORO || pomodoroActive) return;
  uint32_t now = millis();
  if (now < pomoIdleNextAt) return;

  const uint16_t RED  = tft.color565(210, 38, 18);
  const uint16_t RED2 = tft.color565(255, 110, 70);
  const uint16_t DIM  = tft.color565(35, 33, 30);
  // "POMODORO" size 5: each char 30px wide, 8 chars = 240px — fits edge to edge
  const int16_t TX = 0, TY = 92;

  switch (pomoIdlePhase) {

    case 0: {  // ── Title appears ────────────────────
      tft.fillScreen(C_DARKBG);
      tft.fillRect(0, 0, DISP_W, 3, C_ORANGE);
      tft.setTextColor(C_WHITE); tft.setTextSize(5);
      tft.setCursor(TX, TY);
      tft.print("POMODORO");
      pomoIdlePhase = 1;
      pomoIdleStep  = 0;
      pomoIdleNextAt = now + 1400;
      break;
    }

    case 1: {  // ── Tomato flies in ──────────────────
      // 15 steps: x from 252 → 12 (240px / 16 steps ≈ 15px each)
      int16_t tx = 252 - pomoIdleStep * 16;
      int16_t ty = 92;

      // Redraw bg + text each frame to erase previous tomato
      tft.fillScreen(C_DARKBG);
      tft.fillRect(0, 0, DISP_W, 3, C_ORANGE);
      tft.setTextColor(C_WHITE); tft.setTextSize(5);
      tft.setCursor(TX, TY);
      tft.print("POMODORO");

      drawTomPixel(tx, ty, 8);

      pomoIdleStep++;
      pomoIdleNextAt = now + 32;
      if (pomoIdleStep >= 16) { pomoIdlePhase = 2; pomoIdleStep = 0; }
      break;
    }

    case 2: {  // ── Impact flash ─────────────────────
      tft.fillScreen(pomoIdleStep % 2 == 0 ? RED : C_DARKBG);
      pomoIdleStep++;
      pomoIdleNextAt = now + 70;
      if (pomoIdleStep >= 4) { pomoIdlePhase = 3; pomoIdleStep = 0; }
      break;
    }

    case 3: {  // ── Pixel splat ───────────────────────
      if (pomoIdleStep == 0) {
        const uint8_t B = 8;
        tft.fillScreen(C_DARKBG);
        tft.fillRect(0, 0, DISP_W, 3, C_ORANGE);

        // Dim ghost text still readable behind splat
        tft.setTextColor(DIM); tft.setTextSize(5);
        tft.setCursor(TX, TY);
        tft.print("POMODORO");

        // ── Pixel art blob ───────────────────
        // 0=skip 1=red 2=highlight
        const uint8_t SPLAT[8][10] = {
          {0,0,1,1,1,0,0,0,0,0},
          {0,1,1,2,1,1,0,0,0,0},
          {1,1,2,1,1,1,1,0,0,0},
          {1,1,1,1,1,1,1,1,0,0},
          {1,1,1,1,1,1,1,0,0,0},
          {0,1,1,1,1,1,0,0,0,0},
          {0,0,1,1,1,0,0,0,0,0},
          {0,0,0,1,0,0,0,0,0,0},
        };
        int16_t bx = 0, by = 84;
        for (uint8_t r = 0; r < 8; r++)
          for (uint8_t c = 0; c < 10; c++) {
            uint8_t v = SPLAT[r][c];
            if (!v) continue;
            tft.fillRect(bx + c*B, by + r*B, B, B, v==2 ? RED2 : RED);
          }

        // Seeds inside blob
        tft.fillRect(18, 100, 5, 9, DIM);
        tft.fillRect(34, 116, 5, 9, DIM);
        tft.fillRect(10, 124, 5, 9, DIM);

        // Drip below blob
        tft.fillRect(24, 148, B, B, RED);
        tft.fillRect(24, 156, B, B, RED);
        tft.fillRect(26, 164, 5, 5, RED);

        // ── Pixel splatter drops ─────────────
        const int16_t P8[][2] = {
          { 96, 68},{120, 60},{144, 76},{168, 92},
          {192, 88},{ 88,148},{112,156},{136,140},
          { 48,152},{ 32, 56},{ 64, 44},{200,112},
        };
        for (uint8_t i = 0; i < 12; i++)
          tft.fillRect(P8[i][0], P8[i][1], B, B, RED);

        const int16_t P4[][2] = {
          {108,52},{152,52},{196,64},{216,100},{40,168},{220,128},
        };
        for (uint8_t i = 0; i < 6; i++)
          tft.fillRect(P4[i][0], P4[i][1], 4, 4, RED);
      }
      pomoIdleStep++;
      pomoIdleNextAt = now + 50;
      if (pomoIdleStep >= 50) { pomoIdlePhase = 0; pomoIdleStep = 0; }
      break;
    }
  }
}

void drawPomodoroStatic() {
  uint16_t phaseCol = pomodoroOnBreak
    ? tft.color565(70, 110, 220)
    : C_ORANGE;

  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 3, phaseCol);

  // Phase badge
  const char* phase = pomodoroOnBreak ? "BREAK" : "FOCUS";
  tft.setTextColor(phaseCol); tft.setTextSize(3);
  tft.setCursor(DISP_W / 2 - strlen(phase) * 9, 16);
  tft.print(phase);

  tft.fillRect(8, 48, 224, 1, tft.color565(35, 33, 30));

  // Bottom hint
  tft.setTextColor(tft.color565(35, 33, 30)); tft.setTextSize(1);
  tft.setCursor(DISP_W / 2 - 36, 220);
  tft.print("p: pause / resume");
}

void drawPomodoroTime(uint8_t mm, uint8_t ss) {
  uint16_t C_DIM = tft.color565(35, 33, 30);

  // Clear dynamic zone
  tft.fillRect(0, 54, DISP_W, 160, C_DARKBG);

  // MM:SS countdown
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
  tft.setTextColor(C_WHITE); tft.setTextSize(5);
  tft.setCursor(45, 68);
  tft.print(buf);

  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(DISP_W / 2 - 27, 120);
  tft.print("remaining");

  // Progress bar — dashes fill as time elapses
  uint32_t phaseLen = pomodoroOnBreak ? pomoBreakMs : pomoWorkMs;
  uint32_t totalSec = phaseLen / 1000;
  uint32_t remainSec = (uint32_t)mm * 60 + ss;
  uint32_t elapsedSec = totalSec > remainSec ? totalSec - remainSec : 0;
  uint8_t  activeDashes = totalSec > 0 ? (uint8_t)(elapsedSec * 12 / totalSec) : 0;
  uint16_t dashCol = pomodoroOnBreak ? tft.color565(70, 110, 220) : C_ORANGE;
  for (uint8_t i = 0; i < 12; i++)
    tft.fillRect(8 + i * 19, 138, 14, 6, i < activeDashes ? dashCol : C_DIM);

  // Phase duration label
  char label[18];
  if (pomodoroOnBreak) snprintf(label, sizeof(label), "of %d min break", (int)(pomoBreakMs / 60000UL));
  else                 snprintf(label, sizeof(label), "of %d min focus", (int)(pomoWorkMs  / 60000UL));
  tft.setTextColor(C_DIM); tft.setTextSize(1);
  tft.setCursor(DISP_W / 2 - strlen(label) * 3, 156);
  tft.print(label);
}

void pomodoroRemaining(uint8_t &mm, uint8_t &ss) {
  uint32_t phaseLen = pomodoroOnBreak ? pomoBreakMs : pomoWorkMs;
  uint32_t elapsed  = millis() - pomodoroPhaseStart;
  uint32_t remainMs = elapsed >= phaseLen ? 0 : phaseLen - elapsed;
  uint16_t remainSec = remainMs / 1000;
  mm = remainSec / 60; ss = remainSec % 60;
}

void updatePomodoro() {
  if (!pomodoroActive) return;
  if (pomodoroRinging) return;

  static uint16_t lastSec = 0xFFFF;
  uint32_t phaseLen = pomodoroOnBreak ? pomoBreakMs : pomoWorkMs;
  if (millis() - pomodoroPhaseStart >= phaseLen) {
    pomodoroOnBreak    = !pomodoroOnBreak;
    pomodoroRinging    = true;
    pomoRingingFlashAt = 0;
    pomoRingStartAt    = millis();
    lastSec = 0xFFFF;
    Serial.println(pomodoroOnBreak ? "POMO_WORK_END" : "POMO_BREAK_END");
  } else {
    if (currentMode != MODE_POMODORO) return;
    uint8_t mm, ss; pomodoroRemaining(mm, ss);
    uint16_t sec = mm * 60 + ss;
    if (sec != lastSec) { lastSec = sec; drawPomodoroTime(mm, ss); }
  }
}

void updatePomoFlash() {
  if (!pomodoroActive || !pomodoroRinging) return;

  // Auto-dismiss after 10s and start the new phase cleanly
  if (millis() - pomoRingStartAt >= 10000UL) {
    pomodoroRinging    = false;
    pomodoroPhaseStart = millis();  // new phase starts from now
    if (currentMode == MODE_POMODORO) {
      drawPomodoroStatic();
      uint8_t mm, ss; pomodoroRemaining(mm, ss);
      drawPomodoroTime(mm, ss);
    }
    return;
  }

  if (millis() - pomoRingingFlashAt < 300) return;
  pomoRingingFlashAt = millis();
  pomoRingingFlashOn = !pomoRingingFlashOn;

  uint16_t bg = pomoRingingFlashOn ? C_ORANGE : C_WHITE;
  uint16_t fg = pomoRingingFlashOn ? C_WHITE  : C_ORANGE;
  tft.fillScreen(bg);
  tft.setTextColor(fg); tft.setTextSize(3);
  tft.setCursor(30, DISP_H / 2 - 12);
  tft.print(pomodoroOnBreak ? "BREAK!" : "FOCUS!");
}

void checkPomodoroTimeout() {
  if (pomoStoppedAt > 0 && currentMode == MODE_POMODORO && !pomodoroActive) {
    if (millis() - pomoStoppedAt >= 10000UL) {
      pomoStoppedAt = 0; // Disarm
      switchMode(MODE_ANIMATION);
    }
  } else {
    pomoStoppedAt = 0; // Disarm if mode changed or timer became active again
  }
}

