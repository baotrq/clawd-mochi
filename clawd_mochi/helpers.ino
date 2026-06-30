// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  HELPERS
// ═════════════════════════════════════════════════════════════

int speedMs(int ms) {
  if (animSpeed == 3) return ms / 2;
  if (animSpeed == 1) return ms * 2;
  return ms;
}

void setBacklight(bool on) {
  backlightOn = on;
  digitalWrite(TFT_BLK, on ? HIGH : LOW);
}

// ── Buzzer ────────────────────────────────────────────────────
// Non-blocking chirp on the passive piezo: tone() with a duration runs
// off a hardware timer and returns immediately, so the ring-screen flash
// loops keep ticking. Call beepStop() when a ring is dismissed so a
// chirp that's still sounding gets cut off cleanly.
void beep(uint16_t freq, uint16_t ms) {
  tone(BUZZER_PIN, freq, ms);
}

void beepStop() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

// Short rising power-on jingle. Blocking on purpose — it only runs once,
// from setup(), before loop() starts, so there's nothing to keep ticking.
void startupChime() {
  static const uint16_t notes[] = { 523, 659, 784, 1047 };  // C5 E5 G5 C6
  for (uint8_t i = 0; i < 4; i++) {
    tone(BUZZER_PIN, notes[i], 110);
    delay(135);
  }
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

void initColours() {
  C_ORANGE = tft.color565(218, 17, 0);
  C_DARKBG = tft.color565(10,  12,  16);
  C_MUTED  = tft.color565(90,  88,  86);
  C_GREEN  = tft.color565(80, 220, 130);
  animBgColor = C_ORANGE;
}

