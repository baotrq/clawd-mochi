// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  ANIMATIONS
// ═════════════════════════════════════════════════════════════

void animNormalEyes() {
  busy = true;
  const int16_t offs[] = {-16, 16, -16, 16, 0};
  for (uint8_t i = 0; i < 5; i++) { drawNormalEyes(offs[i]); delay(speedMs(80)); }
  drawNormalEyes(0, true);  delay(speedMs(100));
  drawNormalEyes(0, false); delay(speedMs(70));
  drawNormalEyes(0, true);  delay(speedMs(70));
  drawNormalEyes(0, false);
  busy = false;
}

void animSquishEyes() {
  busy = true;
  for (uint8_t i = 0; i < 3; i++) {
    drawSquishEyes(false); delay(speedMs(160));
    drawSquishEyes(true);  delay(speedMs(100));
  }
  drawSquishEyes(false);
  busy = false;
}

void animLookAround() {
  busy = true;
  drawNormalEyes(-30); delay(speedMs(350));
  drawNormalEyes(30);  delay(speedMs(350));
  drawNormalEyes(0);   delay(speedMs(200));
  busy = false;
}

void animWink() {
  busy = true;
  const bool leftWink = random(2) == 0;
  drawWinkEyes(leftWink); delay(speedMs(220));
  drawNormalEyes();       delay(speedMs(120));
  busy = false;
}

void animSleepy() {
  busy = true;
  for (int16_t h = EYE_H; h >= 6; h -= 6) { drawDroopyEyes(h); delay(speedMs(45)); }
  delay(speedMs(350));
  for (int16_t h = 6; h <= EYE_H; h += 6) { drawDroopyEyes(h); delay(speedMs(40)); }
  drawNormalEyes();
  busy = false;
}

void animBlink() {
  busy = true;
  drawNormalEyes(0, true);  delay(speedMs(90));
  drawNormalEyes(0, false);
  busy = false;
}

void animDoubleBlink() {
  busy = true;
  for (uint8_t i = 0; i < 2; i++) {
    drawNormalEyes(0, true);  delay(speedMs(80));
    drawNormalEyes(0, false); delay(speedMs(90));
  }
  busy = false;
}

void animSurprised() {
  busy = true;
  for (int16_t h = EYE_H; h <= EYE_H + 24; h += 8) { drawDroopyEyes(h); delay(speedMs(35)); }
  delay(speedMs(300));
  for (int16_t h = EYE_H + 24; h >= EYE_H; h -= 8) { drawDroopyEyes(h); delay(speedMs(35)); }
  drawNormalEyes();
  busy = false;
}

void animSquint() {
  busy = true;
  for (int16_t h = EYE_H; h >= EYE_H / 3; h -= 6) { drawDroopyEyes(h); delay(speedMs(30)); }
  delay(speedMs(300));
  for (int16_t h = EYE_H / 3; h <= EYE_H; h += 6) { drawDroopyEyes(h); delay(speedMs(30)); }
  drawNormalEyes();
  busy = false;
}

void animNod() {
  busy = true;
  const int16_t offs[] = {12, -12, 12, -12, 0};
  for (uint8_t i = 0; i < 5; i++) { drawNormalEyes(0, false, offs[i]); delay(speedMs(110)); }
  busy = false;
}

void animShake() {
  busy = true;
  const int16_t offs[] = {-10, 10, -10, 10, -10, 10, 0};
  for (uint8_t i = 0; i < 7; i++) { drawNormalEyes(offs[i]); delay(speedMs(55)); }
  busy = false;
}

void animRoll() {
  busy = true;
  const int16_t path[][2] = {
    {0, -14}, {14, -10}, {18, 0}, {14, 10}, {0, 14},
    {-14, 10}, {-18, 0}, {-14, -10}, {0, 0},
  };
  for (uint8_t i = 0; i < 9; i++) { drawNormalEyes(path[i][0], false, path[i][1]); delay(speedMs(70)); }
  busy = false;
}

void animCrossEyed() {
  busy = true;
  drawEyesAsym(14, 0, -14, 0); delay(speedMs(450));
  drawNormalEyes();            delay(speedMs(150));
  busy = false;
}

void animTiltConfused() {
  busy = true;
  drawEyesAsym(0, -10, 0, 10); delay(speedMs(250));
  drawEyesAsym(0, 10, 0, -10); delay(speedMs(250));
  drawNormalEyes();
  busy = false;
}

void animExcited() {
  busy = true;
  for (uint8_t i = 0; i < 3; i++) {
    drawNormalEyes(0, false, -10); delay(speedMs(60));
    drawNormalEyes(0, false, 0);   delay(speedMs(60));
  }
  drawNormalEyes(0, true); delay(speedMs(60));
  drawNormalEyes();
  busy = false;
}

void animLogoReveal() {
  busy = true;
  tft.fillScreen(animBgColor);
  for (uint16_t i = 0; i < LOGO_SEG_COUNT; i++) {
    int16_t x1 = pgm_read_word(&LOGO_SEGS[i][0]);
    int16_t y1 = pgm_read_word(&LOGO_SEGS[i][1]);
    int16_t x2 = pgm_read_word(&LOGO_SEGS[i][2]);
    int16_t y2 = pgm_read_word(&LOGO_SEGS[i][3]);
    tft.drawLine(x1, y1, x2, y2, C_WHITE);
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, C_WHITE);
    if (i % 4 == 0) { delay(speedMs(8)); }
  }
  drawLogoFilled(animBgColor, C_WHITE);
  delay(1500);
  busy = false;
}

void drawHeartEye(int16_t x, int16_t y) {
  tft.fillRect(x, y + 5, 10, 10, C_BLACK);       // Top-left lobe
  tft.fillRect(x + 20, y + 5, 10, 10, C_BLACK);  // Top-right lobe
  tft.fillRect(x, y + 15, 30, 15, C_BLACK);      // Middle body
  tft.fillRect(x + 5, y + 30, 20, 10, C_BLACK);  // Lower body
  tft.fillRect(x + 10, y + 40, 10, 10, C_BLACK); // Bottom tip
}

void drawHeartEyesView(bool blink = false) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  if (!blink) {
    drawHeartEye(lx, ey + 10);
    drawHeartEye(rx, ey + 10);
  } else {
    tft.fillRect(lx, ey + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
    tft.fillRect(rx, ey + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
  }
}

void animHeartEyes() {
  busy = true;
  for (uint8_t i = 0; i < 3; i++) {
    drawHeartEyesView(false); delay(speedMs(400));
    drawHeartEyesView(true);  delay(speedMs(100));
  }
  drawHeartEyesView(false);
  busy = false;
}

void drawCrossEye(int16_t x, int16_t y) {
  for (int16_t i = 0; i < 30; i += 6) {
    tft.fillRect(x + i, y + i + 10, 6, 6, C_BLACK);
    tft.fillRect(x + i, y + 30 - i + 10, 6, 6, C_BLACK);
  }
}

void animDizzyEyes() {
  busy = true;
  const int16_t offs[] = {-4, 4, -4, 4, 0};
  for (uint8_t i = 0; i < 5; i++) {
    tft.fillScreen(animBgColor);
    drawCrossEye(eyeLX(offs[i]), eyeY());
    drawCrossEye(eyeRX(offs[i]), eyeY());
    delay(speedMs(150));
  }
  drawNormalEyes();
  busy = false;
}

void drawGlitchEyesView() {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  
  // Cut outs
  for (int i = 0; i < 4; i++) {
    tft.fillRect(lx + random(EYE_W - 8), ey + random(EYE_H - 8), 8, 8, animBgColor);
    tft.fillRect(rx + random(EYE_W - 8), ey + random(EYE_H - 8), 8, 8, animBgColor);
  }
  // Floating dust
  for (int i = 0; i < 3; i++) {
    tft.fillRect(lx - 12 + random(8), ey + random(EYE_H), 8, 8, C_BLACK);
    tft.fillRect(rx + EYE_W + 4 + random(8), ey + random(EYE_H), 8, 8, C_BLACK);
  }
}

void drawHypnoEyesView(uint8_t step) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  
  for (int16_t r = 0; r < 3; r++) {
    int16_t insetX = ((step + r * 6) % 18);
    int16_t insetY = insetX * 2;
    if (insetX < EYE_W / 2 && insetY < EYE_H / 2) {
      tft.drawRect(lx + insetX, ey + insetY, EYE_W - insetX * 2, EYE_H - insetY * 2, animBgColor);
      tft.drawRect(lx + insetX + 1, ey + insetY + 1, EYE_W - insetX * 2 - 2, EYE_H - insetY * 2 - 2, animBgColor);
      tft.drawRect(rx + insetX, ey + insetY, EYE_W - insetX * 2, EYE_H - insetY * 2, animBgColor);
      tft.drawRect(rx + insetX + 1, ey + insetY + 1, EYE_W - insetX * 2 - 2, EYE_H - insetY * 2 - 2, animBgColor);
    }
  }
}

void animHypnoEyes() {
  busy = true;
  for (uint8_t i = 0; i < 24; i++) {
    drawHypnoEyesView(i);
    delay(speedMs(60));
  }
  drawNormalEyes();
  busy = false;
}

void drawCryTearsStatic() {
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  uint16_t tearCol = tft.color565(80, 150, 255);
  int16_t startY = ey + EYE_H;
  
  tft.fillRect(lx + 2, startY, 10, DISP_H - startY, tearCol);
  tft.fillRect(rx + EYE_W - 12, startY, 10, DISP_H - startY, tearCol);
}

void drawCryEyesView(bool blink) {
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  
  if (!blink) {
    // 1. Clear top area of eye socket (y = ey to ey + 24)
    tft.fillRect(lx, ey, EYE_W, 24, animBgColor);
    tft.fillRect(rx, ey, EYE_W, 24, animBgColor);
    
    // 2. Draw smaller black eyes at the bottom (height = 36, y = ey + 24 to ey + 60)
    tft.fillRect(lx, ey + 24, EYE_W, 36, C_BLACK);
    tft.fillRect(rx, ey + 24, EYE_W, 36, C_BLACK);
    
    // 3. Cut off top-outer corners to slant them outwards (sad / \ shape)
    tft.fillTriangle(lx, ey + 24, lx + 12, ey + 24, lx, ey + 24 + 16, animBgColor);
    tft.fillTriangle(rx + EYE_W - 12, ey + 24, rx + EYE_W, ey + 24, rx + EYE_W, ey + 24 + 16, animBgColor);
  } else {
    // 1. Clear eye socket down to bottom blink line (y = ey to ey + 54)
    tft.fillRect(lx, ey, EYE_W, 54, animBgColor);
    tft.fillRect(rx, ey, EYE_W, 54, animBgColor);
    
    // 2. Draw closed lines at the very bottom (y = ey + 54 to ey + 60)
    tft.fillRect(lx, ey + 54, EYE_W, 6, C_BLACK);
    tft.fillRect(rx, ey + 54, EYE_W, 6, C_BLACK);
  }
}

void animCryEyes() {
  busy = true;
  tft.fillScreen(animBgColor);
  drawCryTearsStatic();
  
  for (uint8_t i = 0; i < 4; i++) {
    drawCryEyesView(false);
    delay(speedMs(600));
    drawCryEyesView(true);
    delay(speedMs(180));
  }
  
  drawCryEyesView(false);
  delay(speedMs(800));
  drawNormalEyes();
  busy = false;
}

void drawNekoEyesView(bool blink) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), cy = eyeCY();
  const int16_t lcx = lx + EYE_W / 2;
  const int16_t rcx = rx + EYE_W / 2;
  
  if (!blink) {
    for (int8_t t = -3; t <= 3; t++) {
      tft.drawLine(lcx - 12, cy + 8 + t, lcx, cy - 10 + t, C_BLACK);
      tft.drawLine(lcx, cy - 10 + t, lcx + 12, cy + 8 + t, C_BLACK);
      tft.drawLine(rcx - 12, cy + 8 + t, rcx, cy - 10 + t, C_BLACK);
      tft.drawLine(rcx, cy - 10 + t, rcx + 12, cy + 8 + t, C_BLACK);
    }
  } else {
    tft.fillRect(lx, cy - 3, EYE_W, 6, C_BLACK);
    tft.fillRect(rx, cy - 3, EYE_W, 6, C_BLACK);
  }
}

void animNekoEyes() {
  busy = true;
  for (uint8_t i = 0; i < 3; i++) {
    drawNekoEyesView(false); delay(speedMs(350));
    drawNekoEyesView(true);  delay(speedMs(100));
  }
  drawNekoEyesView(false);   delay(speedMs(200));
  drawNormalEyes();
  busy = false;
}

void animGlitchEyes() {
  busy = true;
  for (uint8_t i = 0; i < 8; i++) {
    drawGlitchEyesView();
    delay(speedMs(80));
  }
  drawNormalEyes();
  busy = false;
}

// Idle animations cycled by dynamic mode — weighted toward the quick ones
void (*const IDLE_ANIMS[])() = {
  animNormalEyes, animNormalEyes,
  animBlink, animBlink,
  animDoubleBlink,
  animSquishEyes,
  animLookAround,
  animWink,
  animSleepy,
  animSurprised,
  animSquint,
  animNod,
  animShake,
  animRoll,
  animCrossEyed,
  animTiltConfused,
  animExcited,
  animLogoReveal,
  animHeartEyes,
  animDizzyEyes,
  animGlitchEyes,
  animHypnoEyes,
  animCryEyes,
  animNekoEyes,
};
const uint8_t IDLE_ANIM_COUNT = sizeof(IDLE_ANIMS) / sizeof(IDLE_ANIMS[0]);

void playRandomIdleAnim() {
  IDLE_ANIMS[random(IDLE_ANIM_COUNT)]();
  uint32_t base = idleIntervalSec * 1000UL;
  nextIdleAnimAt = millis() + random(base * 3 / 4, base * 5 / 4);
}


