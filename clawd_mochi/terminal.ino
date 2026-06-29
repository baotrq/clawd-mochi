// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  TERMINAL
// ═════════════════════════════════════════════════════════════

void termClear() {
  for (uint8_t i = 0; i < TERM_ROWS; i++) termLines[i] = "";
  termRow = 0; termCol = 0;
}

void termDrawHeader() {
  tft.fillRect(0, 0, DISP_W, TERM_PAD_Y + 1, C_DARKBG);
  tft.setTextColor(C_ORANGE); tft.setTextSize(1);
  tft.setCursor(TERM_PAD_X, 4); tft.print("clawd@mochi terminal");
  tft.drawFastHLine(0, TERM_PAD_Y, DISP_W, C_ORANGE);
}

// Prefix "clawd:~$ " in green, drawn only when the row has content
void termDrawPrefix(int16_t yy) {
  tft.setTextColor(C_GREEN); tft.setTextSize(1);
  tft.setCursor(TERM_PAD_X, yy + 6);
  tft.print("clawd:~$ ");
}

#define PREFIX_PX 54   // 9 chars × 6px = 54px at textSize 1

void termDrawLine(uint8_t r) {
  const int16_t yy = TERM_PAD_Y + 4 + r * TERM_CHAR_H;
  tft.fillRect(0, yy, DISP_W, TERM_CHAR_H, C_DARKBG);
  // show prefix only on the currently active (cursor) line
  if (r == termRow) termDrawPrefix(yy);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(TERM_PAD_X + PREFIX_PX, yy + 1);
  tft.print(termLines[r]);
  if (r == termRow) {
    const int16_t cx = TERM_PAD_X + PREFIX_PX + termCol * TERM_CHAR_W;
    tft.fillRect(cx, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
  }
}

void termDrawLastChar() {
  if (termCol == 0) return;
  const int16_t yy    = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
  const int16_t baseX = TERM_PAD_X + PREFIX_PX;
  const uint8_t prev  = termCol - 1;
  // erase prev cell (had cursor block)
  tft.fillRect(baseX + prev * TERM_CHAR_W, yy + 1, TERM_CHAR_W, TERM_CHAR_H - 1, C_DARKBG);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(baseX + prev * TERM_CHAR_W, yy + 1);
  tft.print(termLines[termRow][prev]);
  // new cursor
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
}

void termDrawBackspace() {
  const int16_t yy    = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
  const int16_t baseX = TERM_PAD_X + PREFIX_PX;
  // erase deleted char + old cursor
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W * 2, TERM_CHAR_H - 1, C_DARKBG);
  // new cursor
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
  // if line now empty, erase the prefix too
  if (termLines[termRow].length() == 0) {
    tft.fillRect(0, yy, TERM_PAD_X + PREFIX_PX, TERM_CHAR_H, C_DARKBG);
  }
}

void termFullRedraw() {
  tft.fillScreen(C_DARKBG);
  termDrawHeader();
  for (uint8_t r = 0; r < TERM_ROWS; r++) termDrawLine(r);
}

void termScroll() {
  for (uint8_t i = 0; i < TERM_ROWS - 1; i++) termLines[i] = termLines[i + 1];
  termLines[TERM_ROWS - 1] = "";
  termRow = TERM_ROWS - 1;
  termFullRedraw();
}

void termAddChar(char c) {
  if (c == '\n' || c == '\r') {
    const int16_t yy = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
    // erase cursor on current row
    tft.fillRect(TERM_PAD_X + PREFIX_PX + termCol * TERM_CHAR_W,
                 yy + 1, TERM_CHAR_W, TERM_CHAR_H - 1, C_DARKBG);
    termRow++; termCol = 0;
    if (termRow >= TERM_ROWS) { termScroll(); return; }
    termDrawLine(termRow);  // draws prefix on new line
  } else if (c == '\b' || c == 127) {
    if (termCol > 0) {
      termCol--;
      termLines[termRow].remove(termLines[termRow].length() - 1);
      termDrawBackspace();
    }
  } else if (c >= 32 && c < 127) {
    if (termCol >= TERM_COLS) {
      termRow++; termCol = 0;
      if (termRow >= TERM_ROWS) { termScroll(); return; }
    }
    // draw prefix on first char of this line
    if (termCol == 0) termDrawPrefix(TERM_PAD_Y + 4 + termRow * TERM_CHAR_H);
    termLines[termRow] += c;
    termCol++;
    termDrawLastChar();
  }
}

void drawCowsayView(String msg) {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  
  const uint8_t maxBubbleWidth = 28;
  
  String words[30];
  uint8_t wordCount = 0;
  int startIdx = 0;
  int spaceIdx = msg.indexOf(' ');
  while (spaceIdx != -1 && wordCount < 30) {
    String w = msg.substring(startIdx, spaceIdx);
    w.trim();
    if (w.length() > 0) {
      words[wordCount++] = w;
    }
    startIdx = spaceIdx + 1;
    spaceIdx = msg.indexOf(' ', startIdx);
  }
  if (startIdx < msg.length() && wordCount < 30) {
    String w = msg.substring(startIdx);
    w.trim();
    if (w.length() > 0) {
      words[wordCount++] = w;
    }
  }
  
  String lines[10];
  uint8_t lineCount = 0;
  String currentLine = "";
  for (uint8_t i = 0; i < wordCount; i++) {
    String testLine = currentLine + (currentLine.length() > 0 ? " " : "") + words[i];
    if (testLine.length() <= maxBubbleWidth) {
      currentLine = testLine;
    } else {
      if (currentLine.length() > 0) {
        lines[lineCount++] = currentLine;
      }
      currentLine = words[i];
      while (currentLine.length() > maxBubbleWidth && lineCount < 10) {
        lines[lineCount++] = currentLine.substring(0, maxBubbleWidth);
        currentLine = currentLine.substring(maxBubbleWidth);
      }
    }
  }
  if (currentLine.length() > 0 && lineCount < 10) {
    lines[lineCount++] = currentLine;
  }
  if (lineCount == 0) {
    lines[lineCount++] = "moo";
  }
  
  uint8_t maxLen = 0;
  for (uint8_t i = 0; i < lineCount; i++) {
    if (lines[i].length() > maxLen) {
      maxLen = lines[i].length();
    }
  }
  
  tft.setTextColor(C_WHITE); tft.setTextSize(1);
  
  int16_t y = 20;
  
  tft.setCursor(10, y);
  tft.print("  ");
  for (uint8_t i = 0; i < maxLen + 2; i++) tft.print("_");
  y += 10;
  
  if (lineCount == 1) {
    tft.setCursor(10, y);
    tft.print("< " + lines[0] + " >");
    y += 10;
  } else {
    for (uint8_t i = 0; i < lineCount; i++) {
      tft.setCursor(10, y);
      char left = '|', right = '|';
      if (i == 0) { left = '/'; right = '\\'; }
      else if (i == lineCount - 1) { left = '\\'; right = '/'; }
      tft.print(left);
      tft.print(" ");
      tft.print(lines[i]);
      for (uint8_t j = lines[i].length(); j < maxLen; j++) tft.print(" ");
      tft.print(" ");
      tft.print(right);
      y += 10;
    }
  }
  
  tft.setCursor(10, y);
  tft.print("  ");
  for (uint8_t i = 0; i < maxLen + 2; i++) tft.print("-");
  y += 10;
  
  const char* cowLines[] = {
    "        \\   ^__^",
    "         \\  (oo)\\_______",
    "            (__)\\       )\\/\\",
    "                ||----w |",
    "                ||     ||"
  };
  
  for (uint8_t i = 0; i < 5; i++) {
    tft.setCursor(10, y);
    tft.print(cowLines[i]);
    y += 10;
  }
  
  tft.setTextColor(C_MUTED);
  tft.setCursor(10, 220);
  tft.print("press any key to return");
}

