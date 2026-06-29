// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  EYES
// ═════════════════════════════════════════════════════════════

inline int16_t eyeLX(int16_t ox) {
  return (DISP_W - (EYE_W * 2 + EYE_GAP)) / 2 + EYE_OX + ox;
}
inline int16_t eyeRX(int16_t ox) { return eyeLX(ox) + EYE_W + EYE_GAP; }
inline int16_t eyeY()            { return (DISP_H - EYE_H) / 2 - EYE_OY; }
inline int16_t eyeCY()           { return eyeY() + EYE_H / 2; }

void drawNormalEyes(int16_t ox, bool blink, int16_t oy) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(ox), rx = eyeRX(ox), ey = eyeY() + oy;
  if (!blink) {
    tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
    tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  } else {
    tft.fillRect(lx, ey + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
    tft.fillRect(rx, ey + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
  }
}

// Independent per-eye offsets, for cross-eyed / confused-tilt looks
void drawEyesAsym(int16_t lxOff, int16_t lyOff, int16_t rxOff, int16_t ryOff, bool blink) {
  tft.fillScreen(animBgColor);
  const int16_t baseLX = eyeLX(0), baseRX = eyeRX(0), baseY = eyeY();
  const int16_t lx = baseLX + lxOff, ly = baseY + lyOff;
  const int16_t rx = baseRX + rxOff, ry = baseY + ryOff;
  if (!blink) {
    tft.fillRect(lx, ly, EYE_W, EYE_H, C_BLACK);
    tft.fillRect(rx, ry, EYE_W, EYE_H, C_BLACK);
  } else {
    tft.fillRect(lx, ly + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
    tft.fillRect(rx, ry + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
  }
}

void drawChevron(int16_t cx, int16_t cy, int16_t arm, int16_t reach,
                 uint8_t thk, bool rightFacing, uint16_t col) {
  for (int8_t t = -(int8_t)thk; t <= (int8_t)thk; t++) {
    if (rightFacing) {
      tft.drawLine(cx - reach/2, cy - arm + t, cx + reach/2, cy + t,      col);
      tft.drawLine(cx + reach/2, cy + t,       cx - reach/2, cy + arm + t, col);
    } else {
      tft.drawLine(cx + reach/2, cy - arm + t, cx - reach/2, cy + t,      col);
      tft.drawLine(cx - reach/2, cy + t,       cx + reach/2, cy + arm + t, col);
    }
  }
}

void drawSquishEyes(bool closed) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), cy = eyeCY();
  const int16_t arm   = EYE_H / 2;
  const int16_t reach = EYE_W / 2;
  const int16_t lcx   = lx + EYE_W / 2;
  const int16_t rcx   = rx + EYE_W / 2;
  if (!closed) {
    drawChevron(lcx, cy, arm, reach, 10, true,  C_BLACK);
    drawChevron(rcx, cy, arm, reach, 10, false, C_BLACK);
  } else {
    tft.fillRect(lx, cy - 5, EYE_W, 10, C_BLACK);
    tft.fillRect(rx, cy - 5, EYE_W, 10, C_BLACK);
  }
}

void drawWinkEyes(bool leftWink) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  tft.fillRect(leftWink ? lx : rx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(leftWink ? rx : lx, cy - 3, EYE_W, 6, C_BLACK);
}

void drawDroopyEyes(int16_t openH) {
  const int16_t lx = eyeLX(0), rx = eyeRX(0), cy = eyeCY();
  const int16_t maxHalf = (EYE_H + 30) / 2;  // covers shrink (sleepy) and grow (surprised)
  const int16_t halfH   = max((int16_t)3, openH) / 2;
  // Only repaint the eye bounding box, not the whole screen, so the
  // shrink/grow steps don't flash/jitter the full display each frame.
  tft.fillRect(lx, cy - maxHalf, EYE_W, maxHalf * 2, animBgColor);
  tft.fillRect(rx, cy - maxHalf, EYE_W, maxHalf * 2, animBgColor);
  tft.fillRect(lx, cy - halfH,   EYE_W, halfH * 2,   C_BLACK);
  tft.fillRect(rx, cy - halfH,   EYE_W, halfH * 2,   C_BLACK);
}

