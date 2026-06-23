/*
 * ╔══════════════════════════════════════════════════════════════╗
 *   CLAWD MOCHI — ESP32-C3 Super Mini + ST7789 1.54" 240×240
 *
 *   Wiring:
 *     SDA → GPIO 10  (hardware SPI MOSI)
 *     SCL → GPIO 8   (hardware SPI SCK)
 *     RST → GPIO 4
 *     DC  → GPIO 3
 *     CS  → GPIO 2
 *     BL  → GPIO 1
 *     VCC → 3V3
 *     GND → GND
 *
 *   Control: Serial Monitor @ 115200 baud (always works), plus a
 *     best-effort WiFi AP + web UI — this board's radio has previously
 *     tested as unreliable/defective, so Serial remains the fallback.
 *     AP SSID/password: see AP_SSID / AP_PASS below. IP printed at boot.
 *     w = normal eyes   s = squish eyes   d = terminal   a = logo
 *     1/2/3 = speed      b = toggle backlight     m = dynamic mode
 *     c = show clock     t = set time (HHMM)      p = pomodoro start/stop
 *     r = set alarm (minutes from now, screen flashes when it rings)
 *     single-shot expressions: e=blink f=double-blink g=look-around
 *       h=wink i=sleepy j=surprised k=squint l=nod n=shake o=roll
 *       u=cross-eyed v=tilt-confused x=excited
 *     in terminal: type "exit" + Enter to leave
 *     (no RTC — clock runs off millis(), defaults to 00:00 at boot)
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>

// ── WiFi (best-effort — this board's radio has previously tested as
//    unreliable/defective; Serial control above remains the fallback) ──
const char* AP_SSID = "ClawdMochi";
const char* AP_PASS = "mochi1234";
WebServer   server(80);

// ── Pins ──────────────────────────────────────────────────────
#define TFT_CS  2
#define TFT_DC  3
#define TFT_RST 4
#define TFT_BLK 1

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ── Display ───────────────────────────────────────────────────
#define DISP_W 240
#define DISP_H 240

// ── Eye constants (shared by both eye views) ──────────────────
#define EYE_W   30
#define EYE_H   60
#define EYE_GAP 120
#define EYE_OX  0     // horizontal offset
#define EYE_OY  40    // vertical offset upward (subtracted from centre)

// ── Colours ───────────────────────────────────────────────────
uint16_t C_ORANGE, C_DARKBG, C_MUTED, C_GREEN;
#define C_WHITE ST77XX_WHITE
#define C_BLACK ST77XX_BLACK

// ── State ─────────────────────────────────────────────────────
#define VIEW_EYES_NORMAL 0
#define VIEW_EYES_SQUISH 1
#define VIEW_CODE        2
#define VIEW_CLOCK       3
#define VIEW_INPUT       4
#define VIEW_POMODORO    5

uint8_t  currentView  = VIEW_EYES_NORMAL;
bool     busy         = false;
bool     backlightOn  = true;
uint8_t  animSpeed    = 1;   // 1=slow(default) 2=normal 3=fast

bool     dynamicMode    = false;  // auto-cycles idle animations when true
uint32_t nextIdleAnimAt = 0;

uint16_t animBgColor  = 0;   // background for eye/logo animations

// ── Clock (no RTC/WiFi — tracked from millis(), defaults to 00:00 at boot
//    unless set with 't'; behaves as a stopwatch until then) ───────────
uint32_t clockBaseMillis   = 0;
uint16_t clockBaseMinutes  = 0;   // minutes-since-midnight at clockBaseMillis
uint32_t nextClockShowAt   = 0;   // next auto popup, every 30 min

// ── Pomodoro timer ───────────────────────────────────────────────────
bool     pomodoroActive    = false;
bool     pomodoroOnBreak   = false;
uint32_t pomodoroPhaseStart = 0;
const uint32_t POMO_WORK_MS  = 25UL * 60000;
const uint32_t POMO_BREAK_MS =  5UL * 60000;

// ── Alarm ────────────────────────────────────────────────────────────
bool     alarmArmed    = false;
bool     alarmRinging  = false;
uint32_t alarmAtMillis = 0;
uint32_t alarmFlashAt  = 0;
bool     alarmFlashOn  = false;

// ── Numeric input prompt (used by 't' set-clock and 'r' set-alarm) ───
enum InputKind { INPUT_NONE, INPUT_CLOCK_SET, INPUT_ALARM_MIN };
InputKind inputKind = INPUT_NONE;
String    inputBuf  = "";

// ── Terminal ──────────────────────────────────────────────────
#define TERM_COLS      15
#define TERM_ROWS       8
#define TERM_CHAR_W    12
#define TERM_CHAR_H    20
#define TERM_PAD_X      8
#define TERM_PAD_Y     18

bool    termMode      = false;
String  termLines[TERM_ROWS];
uint8_t termRow       = 0;
uint8_t termCol       = 0;

// ── Logo data ─────────────────────────────────────────────────
#define LOGO_CX 120
#define LOGO_CY 105

#define LOGO_TRI_COUNT 162
static const int16_t LOGO_TRIS[][6] PROGMEM = {
  {120,105,65,134,100,114},{120,105,100,114,101,113},{120,105,101,113,100,112},
  {120,105,100,112,99,112},{120,105,99,112,93,111},{120,105,93,111,73,111},
  {120,105,73,111,55,110},{120,105,55,110,38,109},{120,105,38,109,34,108},
  {120,105,34,108,30,103},{120,105,30,103,30,100},{120,105,30,100,34,98},
  {120,105,34,98,39,98},{120,105,39,98,50,99},{120,105,50,99,67,100},
  {120,105,67,100,80,101},{120,105,80,101,98,103},{120,105,98,103,101,103},
  {120,105,101,103,101,102},{120,105,101,102,100,101},{120,105,100,101,100,100},
  {120,105,100,100,82,88},{120,105,82,88,63,76},{120,105,63,76,53,69},
  {120,105,53,69,48,65},{120,105,48,65,45,61},{120,105,45,61,44,54},
  {120,105,44,54,49,49},{120,105,49,49,55,49},{120,105,55,49,57,49},
  {120,105,57,49,64,55},{120,105,64,55,78,66},{120,105,78,66,96,79},
  {120,105,96,79,99,81},{120,105,99,81,100,81},{120,105,100,81,100,80},
  {120,105,100,80,99,78},{120,105,99,78,89,60},{120,105,89,60,78,41},
  {120,105,78,41,73,34},{120,105,73,34,72,29},{120,105,72,29,72,28},
  {120,105,72,28,72,27},{120,105,72,27,71,26},{120,105,71,26,71,25},
  {120,105,71,25,71,24},{120,105,71,24,77,16},{120,105,77,16,80,15},
  {120,105,80,15,87,16},{120,105,87,16,91,19},{120,105,91,19,95,29},
  {120,105,95,29,103,46},{120,105,103,46,114,68},{120,105,114,68,118,75},
  {120,105,118,75,119,81},{120,105,119,81,120,83},{120,105,120,83,121,83},
  {120,105,121,83,121,82},{120,105,121,82,122,69},{120,105,122,69,124,54},
  {120,105,124,54,126,34},{120,105,126,34,126,28},{120,105,126,28,129,21},
  {120,105,129,21,135,18},{120,105,135,18,139,20},{120,105,139,20,143,25},
  {120,105,143,25,142,28},{120,105,142,28,140,42},{120,105,140,42,136,64},
  {120,105,136,64,133,78},{120,105,133,78,135,78},{120,105,135,78,136,76},
  {120,105,136,76,144,67},{120,105,144,67,156,51},{120,105,156,51,162,45},
  {120,105,162,45,168,38},{120,105,168,38,172,35},{120,105,172,35,180,35},
  {120,105,180,35,185,43},{120,105,185,43,183,52},{120,105,183,52,175,62},
  {120,105,175,62,168,71},{120,105,168,71,159,83},{120,105,159,83,153,94},
  {120,105,153,94,154,94},{120,105,154,94,155,94},{120,105,155,94,176,90},
  {120,105,176,90,188,88},{120,105,188,88,201,85},{120,105,201,85,208,88},
  {120,105,208,88,208,91},{120,105,208,91,206,97},{120,105,206,97,191,101},
  {120,105,191,101,174,104},{120,105,174,104,148,110},{120,105,148,110,148,111},
  {120,105,148,111,148,111},{120,105,148,111,160,112},{120,105,160,112,165,112},
  {120,105,165,112,177,112},{120,105,177,112,200,114},{120,105,200,114,205,118},
  {120,105,205,118,209,123},{120,105,209,123,208,126},{120,105,208,126,199,131},
  {120,105,199,131,187,128},{120,105,187,128,159,121},{120,105,159,121,149,119},
  {120,105,149,119,147,119},{120,105,147,119,147,120},{120,105,147,120,156,128},
  {120,105,156,128,170,141},{120,105,170,141,189,158},{120,105,189,158,190,163},
  {120,105,190,163,188,166},{120,105,188,166,185,166},{120,105,185,166,169,153},
  {120,105,169,153,162,148},{120,105,162,148,148,136},{120,105,148,136,147,136},
  {120,105,147,136,147,137},{120,105,147,137,150,142},{120,105,150,142,168,168},
  {120,105,168,168,169,176},{120,105,169,176,168,179},{120,105,168,179,163,180},
  {120,105,163,180,158,179},{120,105,158,179,148,165},{120,105,148,165,137,149},
  {120,105,137,149,129,134},{120,105,129,134,128,135},{120,105,128,135,123,189},
  {120,105,123,189,120,192},{120,105,120,192,115,194},{120,105,115,194,110,191},
  {120,105,110,191,108,185},{120,105,108,185,110,174},{120,105,110,174,113,160},
  {120,105,113,160,116,148},{120,105,116,148,118,134},{120,105,118,134,119,129},
  {120,105,119,129,119,129},{120,105,119,129,118,129},{120,105,118,129,107,144},
  {120,105,107,144,91,166},{120,105,91,166,78,180},{120,105,78,180,75,181},
  {120,105,75,181,70,178},{120,105,70,178,70,173},{120,105,70,173,73,169},
  {120,105,73,169,91,146},{120,105,91,146,102,132},{120,105,102,132,109,124},
  {120,105,109,124,109,123},{120,105,109,123,108,123},{120,105,108,123,61,153},
  {120,105,61,153,52,155},{120,105,52,155,49,151},{120,105,49,151,49,146},
  {120,105,49,146,51,144},{120,105,51,144,65,134},{120,105,65,134,65,134},
};

#define LOGO_SEG_COUNT 162
static const int16_t LOGO_SEGS[][4] PROGMEM = {
  {65,134,100,114},{100,114,101,113},{101,113,100,112},{100,112,99,112},
  {99,112,93,111},{93,111,73,111},{73,111,55,110},{55,110,38,109},
  {38,109,34,108},{34,108,30,103},{30,103,30,100},{30,100,34,98},
  {34,98,39,98},{39,98,50,99},{50,99,67,100},{67,100,80,101},
  {80,101,98,103},{98,103,101,103},{101,103,101,102},{101,102,100,101},
  {100,101,100,100},{100,100,82,88},{82,88,63,76},{63,76,53,69},
  {53,69,48,65},{48,65,45,61},{45,61,44,54},{44,54,49,49},
  {49,49,55,49},{55,49,57,49},{57,49,64,55},{64,55,78,66},
  {78,66,96,79},{96,79,99,81},{99,81,100,81},{100,81,100,80},
  {100,80,99,78},{99,78,89,60},{89,60,78,41},{78,41,73,34},
  {73,34,72,29},{72,29,72,28},{72,28,72,27},{72,27,71,26},
  {71,26,71,25},{71,25,71,24},{71,24,77,16},{77,16,80,15},
  {80,15,87,16},{87,16,91,19},{91,19,95,29},{95,29,103,46},
  {103,46,114,68},{114,68,118,75},{118,75,119,81},{119,81,120,83},
  {120,83,121,83},{121,83,121,82},{121,82,122,69},{122,69,124,54},
  {124,54,126,34},{126,34,126,28},{126,28,129,21},{129,21,135,18},
  {135,18,139,20},{139,20,143,25},{143,25,142,28},{142,28,140,42},
  {140,42,136,64},{136,64,133,78},{133,78,135,78},{135,78,136,76},
  {136,76,144,67},{144,67,156,51},{156,51,162,45},{162,45,168,38},
  {168,38,172,35},{172,35,180,35},{180,35,185,43},{185,43,183,52},
  {183,52,175,62},{175,62,168,71},{168,71,159,83},{159,83,153,94},
  {153,94,154,94},{154,94,155,94},{155,94,176,90},{176,90,188,88},
  {188,88,201,85},{201,85,208,88},{208,88,208,91},{208,91,206,97},
  {206,97,191,101},{191,101,174,104},{174,104,148,110},{148,110,148,111},
  {148,111,148,111},{148,111,160,112},{160,112,165,112},{165,112,177,112},
  {177,112,200,114},{200,114,205,118},{205,118,209,123},{209,123,208,126},
  {208,126,199,131},{199,131,187,128},{187,128,159,121},{159,121,149,119},
  {149,119,147,119},{147,119,147,120},{147,120,156,128},{156,128,170,141},
  {170,141,189,158},{189,158,190,163},{190,163,188,166},{188,166,185,166},
  {185,166,169,153},{169,153,162,148},{162,148,148,136},{148,136,147,136},
  {147,136,147,137},{147,137,150,142},{150,142,168,168},{168,168,169,176},
  {169,176,168,179},{168,179,163,180},{163,180,158,179},{158,179,148,165},
  {148,165,137,149},{137,149,129,134},{129,134,128,135},{128,135,123,189},
  {123,189,120,192},{120,192,115,194},{115,194,110,191},{110,191,108,185},
  {108,185,110,174},{110,174,113,160},{113,160,116,148},{116,148,118,134},
  {118,134,119,129},{119,129,119,129},{119,129,118,129},{118,129,107,144},
  {107,144,91,166},{91,166,78,180},{78,180,75,181},{75,181,70,178},
  {70,178,70,173},{70,173,73,169},{73,169,91,146},{91,146,102,132},
  {102,132,109,124},{109,124,109,123},{109,123,108,123},{108,123,61,153},
  {61,153,52,155},{52,155,49,151},{49,151,49,146},{49,146,51,144},
  {51,144,65,134},{65,134,65,134},
};

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

void initColours() {
  // C_ORANGE = tft.color565(170, 72, 28);
  C_ORANGE = tft.color565(218, 17, 0);
  C_DARKBG = tft.color565(10,  12,  16);
  C_MUTED  = tft.color565(90,  88,  86);
  C_GREEN  = tft.color565(80, 220, 130);
  animBgColor = C_ORANGE;
}

// ═════════════════════════════════════════════════════════════
//  LOGO
// ═════════════════════════════════════════════════════════════

void drawLogoFilled(uint16_t bg, uint16_t fg) {
  tft.fillScreen(bg);
  for (uint16_t i = 0; i < LOGO_TRI_COUNT; i++) {
    tft.fillTriangle(
      pgm_read_word(&LOGO_TRIS[i][0]), pgm_read_word(&LOGO_TRIS[i][1]),
      pgm_read_word(&LOGO_TRIS[i][2]), pgm_read_word(&LOGO_TRIS[i][3]),
      pgm_read_word(&LOGO_TRIS[i][4]), pgm_read_word(&LOGO_TRIS[i][5]),
      fg);
  }
  tft.setTextColor(fg); tft.setTextSize(2);
  tft.setCursor(LOGO_CX - 54, 210); tft.print("Anthropic");
  tft.setCursor(LOGO_CX - 53, 210); tft.print("Anthropic");
}

// ═════════════════════════════════════════════════════════════
//  VIEWS
// ═════════════════════════════════════════════════════════════

// Eye helpers — shared constants via #define EYE_*
inline int16_t eyeLX(int16_t ox) {
  return (DISP_W - (EYE_W * 2 + EYE_GAP)) / 2 + EYE_OX + ox;
}
inline int16_t eyeRX(int16_t ox) { return eyeLX(ox) + EYE_W + EYE_GAP; }
inline int16_t eyeY()            { return (DISP_H - EYE_H) / 2 - EYE_OY; }
inline int16_t eyeCY()           { return eyeY() + EYE_H / 2; }

void drawNormalEyes(int16_t ox = 0, bool blink = false, int16_t oy = 0) {
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
void drawEyesAsym(int16_t lxOff, int16_t lyOff, int16_t rxOff, int16_t ryOff, bool blink = false) {
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

void drawSquishEyes(bool closed = false) {
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

void drawCodeView() {
  termMode = false;
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0,          DISP_W, 4, C_ORANGE);
  tft.fillRect(0, DISP_H - 4, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_ORANGE); tft.setTextSize(4);
  tft.setCursor((DISP_W - 144) / 2, DISP_H / 2 - 52); tft.print("Claude");
  tft.setTextColor(C_WHITE);  tft.setTextSize(4);
  tft.setCursor((DISP_W - 96) / 2,  DISP_H / 2 + 8);  tft.print("Code");
  tft.fillRect((DISP_W - 96) / 2, DISP_H / 2 + 52, 96, 3, C_ORANGE);
}

// ═════════════════════════════════════════════════════════════
//  CLOCK
// ═════════════════════════════════════════════════════════════

uint16_t clockNowMinutes() {
  uint32_t elapsedMin = (millis() - clockBaseMillis) / 60000UL;
  return (clockBaseMinutes + elapsedMin) % 1440;
}

void drawClockView() {
  uint16_t m = clockNowMinutes();
  uint8_t hh = m / 60, mm = m % 60;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_WHITE); tft.setTextSize(6);
  tft.setCursor(DISP_W / 2 - 72, DISP_H / 2 - 24);
  tft.print(buf);
}

// Keep the manually-opened clock view ('c') ticking forward while shown
void updateClockViewIfShown() {
  if (currentView != VIEW_CLOCK) return;
  static uint16_t lastMin = 0xFFFF;
  uint16_t m = clockNowMinutes();
  if (m != lastMin) { lastMin = m; drawClockView(); }
}

// ═════════════════════════════════════════════════════════════
//  NUMERIC INPUT PROMPT ("t" set-clock, "r" set-alarm)
// ═════════════════════════════════════════════════════════════

void drawInputPrompt() {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(12, 30);
  tft.print(inputKind == INPUT_CLOCK_SET ? "Set time HHMM:" : "Alarm in (min):");
  tft.fillRect(12, 70, DISP_W - 24, 30, C_DARKBG);
  tft.setTextColor(C_GREEN); tft.setTextSize(3);
  tft.setCursor(12, 70);
  tft.print(inputBuf);
  tft.print("_");
}

void enterTimeInput(InputKind kind) {
  inputKind = kind;
  inputBuf  = "";
  currentView = VIEW_INPUT;
  drawInputPrompt();
}

void cancelInput() {
  inputKind = INPUT_NONE;
  inputBuf  = "";
  currentView = VIEW_EYES_NORMAL;
  drawNormalEyes();
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
    clockBaseMinutes = hh * 60 + mm;
    clockBaseMillis  = millis();
    Serial.print("Clock set to "); Serial.println(inputBuf);
  } else if (inputKind == INPUT_ALARM_MIN) {
    uint16_t mins = inputBuf.toInt();
    if (mins > 0) {
      armAlarm(mins);
      Serial.print("Alarm armed in "); Serial.print(mins); Serial.println(" min");
    }
  }
  cancelInput();
}

void handleInputChar(char c) {
  if (c == '\n' || c == '\r') {
    submitInput();
  } else if (c == '\b' || c == 127) {
    if (inputBuf.length() > 0) { inputBuf.remove(inputBuf.length() - 1); drawInputPrompt(); }
  } else if (c >= '0' && c <= '9' && inputBuf.length() < 4) {
    inputBuf += c;
    drawInputPrompt();
  }
}

// ═════════════════════════════════════════════════════════════
//  POMODORO TIMER
// ═════════════════════════════════════════════════════════════

void drawPomodoroStatic() {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  tft.setTextColor(pomodoroOnBreak ? C_GREEN : C_ORANGE); tft.setTextSize(2);
  tft.setCursor(20, 30);
  tft.print(pomodoroOnBreak ? "BREAK" : "FOCUS");
}

void drawPomodoroTime(uint8_t mm, uint8_t ss) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
  tft.fillRect(0, DISP_H / 2 - 30, DISP_W, 60, C_DARKBG);
  tft.setTextColor(C_WHITE); tft.setTextSize(5);
  tft.setCursor(DISP_W / 2 - 60, DISP_H / 2 - 20);
  tft.print(buf);
}

void startPomodoro() {
  pomodoroActive  = true;
  pomodoroOnBreak = false;
  pomodoroPhaseStart = millis();
  currentView = VIEW_POMODORO;
  drawPomodoroStatic();
  drawPomodoroTime(25, 0);
}

void stopPomodoro() {
  pomodoroActive = false;
  currentView = VIEW_EYES_NORMAL;
  drawNormalEyes();
}

void updatePomodoro() {
  if (!pomodoroActive) return;
  static uint16_t lastSec = 0xFFFF;
  uint32_t phaseLen = pomodoroOnBreak ? POMO_BREAK_MS : POMO_WORK_MS;
  uint32_t elapsed  = millis() - pomodoroPhaseStart;
  if (elapsed >= phaseLen) {
    pomodoroOnBreak    = !pomodoroOnBreak;
    pomodoroPhaseStart = millis();
    drawPomodoroStatic();
    lastSec = 0xFFFF;
    return;
  }
  uint16_t remainSec = (phaseLen - elapsed) / 1000;
  if (remainSec != lastSec) {
    lastSec = remainSec;
    drawPomodoroTime(remainSec / 60, remainSec % 60);
  }
}

// ═════════════════════════════════════════════════════════════
//  ALARM
// ═════════════════════════════════════════════════════════════

void checkAlarm() {
  if (alarmArmed && millis() >= alarmAtMillis) {
    alarmArmed   = false;
    alarmRinging = true;
    alarmFlashAt = 0;
  }
}

void updateAlarmFlash() {
  if (!alarmRinging) return;
  if (millis() - alarmFlashAt < 300) return;
  alarmFlashAt = millis();
  alarmFlashOn = !alarmFlashOn;
  uint16_t bg = alarmFlashOn ? C_ORANGE : C_WHITE;
  uint16_t fg = alarmFlashOn ? C_WHITE  : C_ORANGE;
  tft.fillScreen(bg);
  tft.setTextColor(fg); tft.setTextSize(3);
  tft.setCursor(30, DISP_H / 2 - 12);
  tft.print("ALARM!");
}

void dismissAlarm() {
  alarmRinging = false;
  currentView  = VIEW_EYES_NORMAL;
  drawNormalEyes();
}

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
};
const uint8_t IDLE_ANIM_COUNT = sizeof(IDLE_ANIMS) / sizeof(IDLE_ANIMS[0]);

void playRandomIdleAnim() {
  IDLE_ANIMS[random(IDLE_ANIM_COUNT)]();
  nextIdleAnimAt = millis() + random(speedMs(1500), speedMs(4000));
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

// ═════════════════════════════════════════════════════════════
//  WIFI WEB UI (best-effort — see note near AP_SSID above)
// ═════════════════════════════════════════════════════════════

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Clawd Mochi</title>
<style>
body{font-family:sans-serif;background:#0a0c10;color:#fff;text-align:center;padding:16px}
button{font-size:18px;padding:12px 16px;margin:4px;border:none;border-radius:8px;background:#da1100;color:#fff}
input{font-size:18px;padding:10px;border-radius:8px;border:none;width:60%}
</style></head><body>
<h2>Clawd Mochi</h2>
<div>
<button onclick="send('w')">Eyes</button>
<button onclick="send('s')">Squish</button>
<button onclick="send('a')">Logo</button>
<button onclick="send('d')">Term</button>
<button onclick="send('m')">Dynamic</button>
<button onclick="send('b')">Backlight</button>
</div>
<div>
<button onclick="send('1')">Slow</button>
<button onclick="send('2')">Normal</button>
<button onclick="send('3')">Fast</button>
</div>
<div>
<button onclick="send('c')">Clock</button>
<button onclick="send('p')">Pomodoro</button>
</div>
<div>
<input id="txt" placeholder="t1430 / r10 / type to terminal">
<button onclick="sendText()">Send</button>
</div>
<script>
function send(c){ fetch('/cmd?c=' + encodeURIComponent(c)); }
function sendText(){
  const v = document.getElementById('txt').value;
  fetch('/cmd?c=' + encodeURIComponent(v + '\n'));
  document.getElementById('txt').value = '';
}
</script>
</body></html>
)HTML";

void routeRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void routeCmd() {
  // Allow the standalone control app (opened from a local file://, a
  // different origin than this device) to call this endpoint directly.
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String c = server.arg("c");
  for (uint16_t i = 0; i < c.length(); i++) handleChar(c[i]);
  server.send(200, "text/plain", "ok");
}

void routeNotFound() {
  server.send(404, "text/plain", "not found");
}

// ═════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  clockBaseMillis = millis();
  nextClockShowAt = millis() + 30UL * 60000;

  pinMode(TFT_BLK, OUTPUT);
  setBacklight(true);

  SPI.begin(8, -1, 10, TFT_CS);   // SCK=8, MOSI=10
  tft.init(240, 240);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);
  initColours();

  // ── WiFi AP (best-effort, non-blocking — Serial control works either way)
  WiFi.mode(WIFI_AP);
  bool apOk = WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4);
  Serial.print("WiFi AP \""); Serial.print(AP_SSID); Serial.print("\": ");
  Serial.println(apOk ? "started" : "failed to start");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  server.on("/", routeRoot);
  server.on("/cmd", routeCmd);
  server.onNotFound(routeNotFound);
  server.begin();

  // ── Boot splash ────────────────────────────────────────────
  tft.fillScreen(animBgColor);
  tft.setTextColor(C_WHITE); tft.setTextSize(3);
  tft.setCursor(DISP_W / 2 - 54, DISP_H / 2 - 22); tft.print("Clawd");
  tft.setCursor(DISP_W / 2 - 54, DISP_H / 2 + 14); tft.print("Mochi");
  delay(1200);

  // ── Logo shown once at startup ─────────────────────────────
  animLogoReveal();

  // ── Serial control info screen ──────────────────────────────
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_WHITE);  tft.setTextSize(2);
  tft.setCursor(12, 16);  tft.print("Serial control");
  tft.setTextColor(C_MUTED);  tft.setTextSize(1);
  tft.setCursor(12, 44);  tft.print("115200 baud");
  tft.setTextColor(C_WHITE);  tft.setTextSize(1);
  tft.setCursor(12, 72);  tft.print("w=eyes  s=squish  d=term");
  tft.setCursor(12, 88);  tft.print("a=logo  1/2/3=speed  b=backlight");
  tft.setCursor(12, 104); tft.print("m=dynamic  c=clock  t=set time");
  tft.setCursor(12, 120); tft.print("p=pomodoro  r=set alarm");
  tft.setTextColor(C_MUTED);  tft.setTextSize(1);
  tft.setCursor(12, 140); tft.print("waiting for serial command...");
  tft.setCursor(12, 160); tft.print(apOk ? "WiFi: " : "WiFi: failed");
  if (apOk) { tft.print(AP_SSID); tft.setCursor(12, 176); tft.print(WiFi.softAPIP()); }

  Serial.println("Clawd Mochi ready. Commands:");
  Serial.println("  w=normal eyes  s=squish eyes  d=terminal  a=logo");
  Serial.println("  1/2/3=speed  b=toggle backlight  m=dynamic mode");
  Serial.println("  c=show clock  t=set time (HHMM)  p=pomodoro start/stop");
  Serial.println("  r=set alarm (minutes from now)");
  Serial.println("  e=blink f=double-blink g=look-around h=wink i=sleepy");
  Serial.println("  j=surprised k=squint l=nod n=shake o=roll u=cross-eyed");
  Serial.println("  v=tilt-confused x=excited");
  Serial.println("  in terminal: type \"exit\" + Enter to leave");
  Serial.println("WiFi web UI (if AP started): connect to the AP above, browse to its IP");
}

// ═════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════

void enterTerminal() {
  currentView = VIEW_CODE; drawCodeView();
  termMode = true; termClear(); termFullRedraw();
}

void exitTerminal() {
  termMode = false;
  currentView = VIEW_EYES_NORMAL; drawNormalEyes();
}

// True while some non-idle mode owns the screen — idle/dynamic animations
// and the 30-min clock popup must stay out of the way during these.
bool inSpecialMode() {
  return termMode || pomodoroActive || alarmRinging ||
         inputKind != INPUT_NONE || currentView == VIEW_CLOCK;
}

void maybeAutoShowClock() {
  if (millis() < nextClockShowAt) return;
  nextClockShowAt = millis() + 30UL * 60000;
  if (inSpecialMode()) return;
  uint8_t prevView = currentView;
  drawClockView();
  delay(4000);
  if (prevView == VIEW_EYES_SQUISH) { currentView = VIEW_EYES_SQUISH; drawSquishEyes(); }
  else                              { currentView = VIEW_EYES_NORMAL; drawNormalEyes(); }
}

// Single source of truth for command dispatch — shared by Serial input
// and the WiFi web UI so both control surfaces behave identically.
void handleChar(char c) {
  if (alarmRinging) { dismissAlarm(); return; }

  if (inputKind != INPUT_NONE) { handleInputChar(c); return; }

  if (termMode) {
    if ((c == '\n' || c == '\r') && termLines[termRow].equalsIgnoreCase("exit")) {
      exitTerminal();
    } else {
      termAddChar(c);
    }
    return;
  }

  if (pomodoroActive) {
    if (c == 'p') stopPomodoro();
    return;
  }

  switch (c) {
    case 'w': currentView = VIEW_EYES_NORMAL; animNormalEyes(); break;
    case 's': currentView = VIEW_EYES_SQUISH; animSquishEyes(); break;
    case 'd': enterTerminal(); break;
    case 'a': currentView = VIEW_EYES_NORMAL; animLogoReveal(); break;
    case '1': case '2': case '3': animSpeed = c - '0'; break;
    case 'b': setBacklight(!backlightOn); break;
    case 'm':
      dynamicMode = !dynamicMode;
      Serial.print("Dynamic mode: "); Serial.println(dynamicMode ? "on" : "off");
      if (dynamicMode) nextIdleAnimAt = millis();
      break;
    case 'c': currentView = VIEW_CLOCK; drawClockView(); break;
    case 't': enterTimeInput(INPUT_CLOCK_SET); break;
    case 'p': startPomodoro(); break;
    case 'r': enterTimeInput(INPUT_ALARM_MIN); break;
    case 'e': currentView = VIEW_EYES_NORMAL; animBlink(); break;
    case 'f': currentView = VIEW_EYES_NORMAL; animDoubleBlink(); break;
    case 'g': currentView = VIEW_EYES_NORMAL; animLookAround(); break;
    case 'h': currentView = VIEW_EYES_NORMAL; animWink(); break;
    case 'i': currentView = VIEW_EYES_NORMAL; animSleepy(); break;
    case 'j': currentView = VIEW_EYES_NORMAL; animSurprised(); break;
    case 'k': currentView = VIEW_EYES_NORMAL; animSquint(); break;
    case 'l': currentView = VIEW_EYES_NORMAL; animNod(); break;
    case 'n': currentView = VIEW_EYES_NORMAL; animShake(); break;
    case 'o': currentView = VIEW_EYES_NORMAL; animRoll(); break;
    case 'u': currentView = VIEW_EYES_NORMAL; animCrossEyed(); break;
    case 'v': currentView = VIEW_EYES_NORMAL; animTiltConfused(); break;
    case 'x': currentView = VIEW_EYES_NORMAL; animExcited(); break;
    default: break;
  }
}

void loop() {
  while (Serial.available()) handleChar(Serial.read());

  server.handleClient();

  checkAlarm();
  updateAlarmFlash();
  updatePomodoro();
  maybeAutoShowClock();
  updateClockViewIfShown();

  if (dynamicMode && !inSpecialMode() && !busy && millis() >= nextIdleAnimAt) {
    currentView = VIEW_EYES_NORMAL;
    playRandomIdleAnim();
  }
}
