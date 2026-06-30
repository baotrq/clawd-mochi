// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  SING MODE — a tiny non-blocking jukebox on the GPIO5 buzzer
//
//  Keys (Sing mode, '7'):
//    q w e r t y → pick & play a song     space / o → play / pause
//    x → stop (silence)                   m → toggle loop
//
//  A song is a list of {frequency Hz, length} pairs. `length` is measured in
//  SIXTEENTH notes, so any rhythm is expressible: 16=whole 8=half 6=dotted-
//  quarter 4=quarter 3=dotted-eighth 2=eighth 1=sixteenth. freq 0 = a rest.
//  Each song carries its own sixteenthMs, so tempo is per-song. updateSing()
//  walks one note per loop() tick off millis(), so the device stays responsive.
// ═════════════════════════════════════════════════════════════

// ── Note pitches (Hz) ─────────────────────────────────────────
#define NR  0     // rest
#define NC4 262
#define ND4 294
#define NE4 330
#define NF4 349
#define NFS4 370  // F#4
#define NG4 392
#define NA4 440
#define NB4 494
#define NC5 523
#define ND5 587
#define NE5 659
#define NF5 698
#define NG5 784

// ── Songs (lengths in sixteenth-notes; see header) ────────────
static const uint16_t twinkle[][2] = {
  {NC4,4},{NC4,4},{NG4,4},{NG4,4},{NA4,4},{NA4,4},{NG4,8},
  {NF4,4},{NF4,4},{NE4,4},{NE4,4},{ND4,4},{ND4,4},{NC4,8},
  {NG4,4},{NG4,4},{NF4,4},{NF4,4},{NE4,4},{NE4,4},{ND4,8},
  {NG4,4},{NG4,4},{NF4,4},{NF4,4},{NE4,4},{NE4,4},{ND4,8},
  {NC4,4},{NC4,4},{NG4,4},{NG4,4},{NA4,4},{NA4,4},{NG4,8},
  {NF4,4},{NF4,4},{NE4,4},{NE4,4},{ND4,4},{ND4,4},{NC4,8},
};

static const uint16_t ode[][2] = {
  {NE4,4},{NE4,4},{NF4,4},{NG4,4},{NG4,4},{NF4,4},{NE4,4},{ND4,4},
  {NC4,4},{NC4,4},{ND4,4},{NE4,4},{NE4,8},{ND4,4},{ND4,8},
  {NE4,4},{NE4,4},{NF4,4},{NG4,4},{NG4,4},{NF4,4},{NE4,4},{ND4,4},
  {NC4,4},{NC4,4},{ND4,4},{NE4,4},{ND4,8},{NC4,4},{NC4,8},
};

static const uint16_t mary[][2] = {
  {NE4,4},{ND4,4},{NC4,4},{ND4,4},{NE4,4},{NE4,4},{NE4,8},
  {ND4,4},{ND4,4},{ND4,8},{NE4,4},{NG4,4},{NG4,8},
  {NE4,4},{ND4,4},{NC4,4},{ND4,4},{NE4,4},{NE4,4},{NE4,4},{NE4,4},
  {ND4,4},{ND4,4},{NE4,4},{ND4,4},{NC4,16},
};

static const uint16_t jingle[][2] = {
  {NE4,4},{NE4,4},{NE4,8},{NE4,4},{NE4,4},{NE4,8},
  {NE4,4},{NG4,4},{NC4,4},{ND4,4},{NE4,16},
  {NF4,4},{NF4,4},{NF4,4},{NF4,4},{NF4,4},{NE4,4},{NE4,4},{NE4,2},{NE4,2},
  {NE4,4},{ND4,4},{ND4,4},{NE4,4},{ND4,8},{NG4,8},
};

static const uint16_t birthday[][2] = {
  {NG4,2},{NG4,2},{NA4,4},{NG4,4},{NC5,4},{NB4,8},
  {NG4,2},{NG4,2},{NA4,4},{NG4,4},{ND5,4},{NC5,8},
  {NG4,2},{NG4,2},{NG5,4},{NE5,4},{NC5,4},{NB4,4},{NA4,8},
  {NF5,2},{NF5,2},{NE5,4},{NC5,4},{ND5,4},{NC5,8},
};

// Tháng Tư Là Lời Nói Dối Của Anh — Hà Anh Tuấn (arr. rguigui / Phạm Toàn Thắng).
// Auto-extracted from the MIDI: the right-hand top-voice melody, reduced to a
// monophonic "skyline" on a 1/16-note grid (the buzzer can't play chords), at
// the score's ♩=65 tempo. Frequencies are raw Hz; lengths in sixteenths.
static const uint16_t thangtu[][2] = {
  {1319,1},{1175,1},{880,11},{0,3},{1319,1},{1175,1},{831,11},{0,3},
  {1175,1},{1047,1},{784,6},{1047,2},{0,1},{622,1},{988,2},{880,4},
  {523,1},{262,1},{349,1},{262,1},{330,1},{262,1},{349,1},{262,1},
  {523,1},{262,1},{349,1},{262,1},{330,1},{262,1},{349,1},{262,1},
  {523,1},{262,1},{392,1},{262,1},{370,1},{262,1},{392,1},{262,1},
  {523,1},{262,1},{392,1},{262,1},{370,1},{262,1},{392,1},{262,1},
  {659,1},{262,1},{440,1},{262,1},{415,1},{262,1},{440,1},{262,1},
  {659,1},{262,1},{440,1},{262,1},{415,1},{262,1},{440,1},{262,1},
  {659,1},{587,1},{523,1},{440,1},{349,1},{294,1},{262,1},{196,1},
  {0,2},{784,13},{587,1},{392,4},{440,1},{494,2},{523,1},{494,4},
  {440,1},{392,3},{440,3},{330,5},{262,4},{494,2},{523,2},{349,4},
  {440,1},{494,1},{523,1},{494,3},{440,1},{392,7},{330,1},{392,5},
  {440,4},{494,1},{523,1},{784,2},{659,4},{440,1},{494,2},{523,1},
  {587,4},{494,1},{440,3},{494,3},{392,5},{523,3},{392,3},{330,2},
  {262,4},{440,1},{494,1},{523,1},{494,3},{440,3},{392,3},{440,4},
  {0,1},{220,1},{294,1},{330,1},{277,6},{659,1},{440,3},{392,1},
  {440,3},{659,2},{440,2},{392,1},{440,4},{523,1},{440,3},{392,3},
  {440,2},{659,7},{440,3},{392,1},{440,3},{659,2},{440,2},{392,1},
  {440,4},{392,4},{330,5},{392,4},{349,2},{659,1},{440,3},{392,1},
  {440,3},{659,2},{440,2},{392,1},{440,4},{523,1},{440,3},{392,2},
  {440,2},{659,4},{523,3},{392,1},{523,1},{494,2},{523,1},{494,3},
  {440,1},{494,3},{440,1},{494,3},{392,4},{440,3},{220,1},{294,1},
  {330,1},{659,5},{587,1},{523,1},{494,1},{440,4},{523,3},{587,3},
  {659,2},{784,2},{659,2},{587,2},{659,1},{784,3},{659,1},{587,2},
  {659,4},{587,1},{523,1},{494,1},{440,4},{523,3},{587,3},{659,4},
  {1047,2},{988,2},{1047,1},{784,4},{659,6},{587,1},{523,1},{494,1},
  {440,4},{523,3},{587,3},{659,2},{784,2},{587,4},{659,1},{784,3},
  {659,1},{587,2},{659,6},{440,4},{659,3},{587,4},{784,3},{880,3},
  {440,4},{294,1},{440,1},{587,1},{659,1},{554,8},
};

struct Song {
  const uint16_t (*notes)[2];
  uint16_t count;
  const char* name;
  char key;
  uint16_t sixteenthMs;   // tempo: ms per sixteenth-note
};

static const Song SONGS[] = {
  { twinkle,  sizeof(twinkle)  / sizeof(twinkle[0]),  "Twinkle Star", 'q', 94 },
  { ode,      sizeof(ode)      / sizeof(ode[0]),      "Ode to Joy",   'w', 94 },
  { mary,     sizeof(mary)     / sizeof(mary[0]),     "Mary's Lamb",  'e', 94 },
  { jingle,   sizeof(jingle)   / sizeof(jingle[0]),   "Jingle Bells", 'r', 94 },
  { birthday, sizeof(birthday) / sizeof(birthday[0]), "Happy Bday",   't', 110 },
  { thangtu,  sizeof(thangtu)  / sizeof(thangtu[0]),  "Thang Tu", 'y', 231 },
};
#define NUM_SONGS ((int8_t)(sizeof(SONGS) / sizeof(SONGS[0])))

// ── Playback engine (non-blocking) ────────────────────────────

// Begin the current note: sound it (slightly clipped for a staccato gap) and
// stamp when it should end. freq 0 → a silent rest of the same length.
static void singStartNote() {
  const Song& sg = SONGS[singSong];
  uint16_t freq  = sg.notes[singNoteIdx][0];
  uint16_t units = sg.notes[singNoteIdx][1];
  singNoteMs = units * sg.sixteenthMs;
  singNoteAt = millis();
  // Small FIXED articulation gap (not proportional) so long sustained notes
  // stay held instead of getting a big silence clipped off their tail.
  uint16_t play = singNoteMs > 45 ? singNoteMs - 25 : singNoteMs;
  if (freq) tone(BUZZER_PIN, freq, play);
  else      noTone(BUZZER_PIN);
}

void startSong(int8_t idx) {
  if (idx < 0 || idx >= NUM_SONGS) return;
  singSong    = idx;
  singNoteIdx = 0;
  singPlaying = true;
  singStartNote();
  if (currentMode == MODE_SING) drawSingView();
}

void stopSong() {
  if (!singPlaying && singNoteMs == 0) { noTone(BUZZER_PIN); return; }
  singPlaying = false;
  singNoteMs  = 0;
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

// Called every loop(): when the current note's time is up, advance to the
// next one (looping or stopping at the end).
void updateSing() {
  if (!singPlaying) return;
  if (millis() - singNoteAt < singNoteMs) return;

  singNoteIdx++;
  if (singNoteIdx >= SONGS[singSong].count) {
    if (singLoop) {
      singNoteIdx = 0;
    } else {
      singPlaying = false;
      noTone(BUZZER_PIN);
      if (currentMode == MODE_SING) drawSingName();
      return;
    }
  }
  singStartNote();
}

// ── View ──────────────────────────────────────────────────────
// The device is a *display only* — songs are chosen from the web app. The
// screen shows the current song's name, a play/pause state, and an elapsed /
// total duration readout with a progress bar (plus a bouncing equalizer).

static uint32_t singTotalMs  = 0;       // total length of the selected song
static uint16_t singLastTick = 0xFFFF;  // last drawn progress tick (¼-second)

static uint32_t songTotalMs(int8_t idx) {
  uint32_t units = 0;
  for (uint16_t i = 0; i < SONGS[idx].count; i++) units += SONGS[idx].notes[i][1];
  return units * SONGS[idx].sixteenthMs;
}

static uint32_t songElapsedMs() {
  uint32_t units = 0;
  for (uint16_t i = 0; i < singNoteIdx; i++) units += SONGS[singSong].notes[i][1];
  uint32_t base  = units * SONGS[singSong].sixteenthMs;
  uint32_t intra = (singPlaying && singNoteMs) ? (millis() - singNoteAt) : 0;
  if (intra > singNoteMs) intra = singNoteMs;
  return base + intra;
}

// Draw one centred line of text at a given size/y.
static void drawCentered(const char* s, uint8_t size, int16_t y) {
  tft.setTextSize(size);
  tft.setCursor((DISP_W - (int16_t)(strlen(s) * 6 * size)) / 2, y);
  tft.print(s);
}

void drawSingName() {
  tft.fillRect(0, 10, DISP_W, 96, C_DARKBG);

  const char* name = SONGS[singSong].name;
  // Biggest text size (up to 4) that fits the title on one static line.
  uint8_t size = 4;
  while (size > 1 && (int)(strlen(name) * 6 * size) > DISP_W - 12) size--;
  tft.setTextColor(C_ORANGE);
  drawCentered(name, size, 44 - size * 4);

  const char* st = singPlaying ? "PLAYING" : "PAUSED";
  tft.setTextColor(singPlaying ? C_GREEN : C_MUTED);
  drawCentered(st, 2, 74);
}

// Elapsed / total time text + the progress-bar fill.
void drawSingProgress() {
  uint32_t el = songElapsedMs();
  if (el > singTotalMs) el = singTotalMs;

  char buf[20];
  snprintf(buf, sizeof(buf), "%lu:%02lu / %lu:%02lu",
           (unsigned long)(el / 60000), (unsigned long)((el / 1000) % 60),
           (unsigned long)(singTotalMs / 60000), (unsigned long)((singTotalMs / 1000) % 60));
  tft.fillRect(0, 112, DISP_W, 22, C_DARKBG);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  int16_t w = strlen(buf) * 12;
  tft.setCursor((DISP_W - w) / 2, 114);
  tft.print(buf);

  int16_t fill = singTotalMs ? (int16_t)((uint64_t)el * 196 / singTotalMs) : 0;
  tft.fillRect(22, 152, 196, 8, C_DARKBG);
  tft.fillRect(22, 152, fill, 8, C_ORANGE);
}

void drawSingView() {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 3, C_ORANGE);

  singTotalMs = songTotalMs(singSong);
  drawSingName();

  tft.drawRect(20, 150, 200, 12, C_MUTED);   // progress-bar frame
  singLastTick = 0xFFFF;
  drawSingProgress();

  for (uint8_t i = 0; i < 12; i++) singBars[i] = 0;
  singBarAt = 0;
}

// Per-tick refresh: live progress (¼-second steps) + bouncing equalizer.
void updateSingView() {
  if (currentMode != MODE_SING) return;

  uint16_t tick = (uint16_t)(songElapsedMs() / 250);
  if (tick != singLastTick) { singLastTick = tick; drawSingProgress(); }

  if (millis() - singBarAt < 90) return;
  singBarAt = millis();

  const int16_t BASE = 234;     // bars grow upward from here
  const int16_t BW = 16, GAP = 4, X0 = 6, MAXH = 24;
  for (uint8_t i = 0; i < 12; i++) {
    uint8_t h = singPlaying ? random(4, MAXH) : 2;
    int16_t x = X0 + i * (BW + GAP);
    // clear the full column, then draw the bar
    tft.fillRect(x, BASE - MAXH, BW, MAXH, C_DARKBG);
    uint16_t col = singPlaying ? (h > MAXH * 2 / 3 ? C_ORANGE : C_GREEN) : C_MUTED;
    tft.fillRect(x, BASE - h, BW, h, col);
    singBars[i] = h;
  }
}

// ── Key handling ──────────────────────────────────────────────
void handleSingKey(char c) {
  // Direct song picks
  for (int8_t i = 0; i < NUM_SONGS; i++) {
    if (c == SONGS[i].key) { startSong(i); return; }
  }
  switch (c) {
    case ' ':
    case 'o':   // play / pause toggle
      if (singPlaying) {
        singPlaying = false;
        noTone(BUZZER_PIN);
      } else {
        singPlaying = true;
        singStartNote();   // resume from current note
      }
      drawSingName();
      break;
    case 'x':   // stop & silence
      stopSong();
      drawSingName();
      break;
    case 'm':   // loop toggle
      singLoop = !singLoop;
      Serial.println(singLoop ? "SING:LOOP_ON" : "SING:LOOP_OFF");
      break;
    default: break;
  }
}
