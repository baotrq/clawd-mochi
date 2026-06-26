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
 *   Control: Serial Monitor @ 115200 baud.
 *
 *   ── MODES (switch anytime, even mid-action) ──────────────────
 *     1 = Animation mode     2 = Clock mode
 *     3 = Pomodoro mode      4 = Terminal mode
 *     5 = Usage mode (Claude Pro 5h/7d usage, see U below)
 *
 *   ── AUTO-CYCLING (Animation mode only — never interrupts 2/3/4/5) ──
 *     Idle loop: Animation (3-5 min) → Clock (3-5 min) → Weather (3-5 min,
 *     skipped until weather data is pushed) → repeat. The web app
 *     auto-refreshes weather every 2 min while in weather mode.
 *     Usage auto-pops on first cross of each 10% mark (10/20/.../90/95)
 *     for 4 s then returns — a nudge as you approach the cap.
 *
 *   ── GLOBAL (work in any mode) ─────────────────────────────────
 *     b = toggle backlight     - / = = speed down / up
 *     U = push Claude usage stats silently (12 digits + Enter:
 *         SS WW RRR TTTTT = session%, weekly%, minutes until session
 *         reset, minutes until weekly reset — e.g. U304801800180\n =
 *         30% / 48% / resets in 18min / resets in 180min; sent
 *         automatically by the web app — only visible in Usage mode)
 *     T = silently sync the clock (HHMM digits + Enter, e.g. T1530\n),
 *         without switching modes or drawing anything — sent by the web
 *         app right after connecting, so the device stays on whatever
 *         it's currently showing (default Animation) while it syncs
 *
 *   ── ANIMATION MODE ────────────────────────────────────────────
 *     w = normal eyes   s = squish eyes   z = logo reveal
 *     m = toggle dynamic mode (auto-cycles idle expressions)
 *     single-shot expressions:
 *       e=blink f=double-blink g=look-around h=wink i=sleepy
 *       j=surprised k=squint l=nod n=shake o=roll u=cross-eyed
 *       v=tilt-confused x=excited
 *
 *   ── CLOCK MODE ────────────────────────────────────────────────
 *     t = set time (HHMM)      r = set alarm (minutes from now,
 *                                   screen flashes when it rings)
 *
 *   ── POMODORO MODE ─────────────────────────────────────────────
 *     p = start/stop (keeps ticking in the background even if you
 *         switch to another mode — switch back to 3 to check it)
 *     P = set durations + start in one shot, silently (5 digits + Enter:
 *         MM SS B = focus minutes, focus seconds, break minutes — e.g.
 *         P24305\n = 24:30 focus / 5 min break, starts immediately)
 *
 *   ── TERMINAL MODE ─────────────────────────────────────────────
 *     type freely; "exit" + Enter returns to Animation mode
 *
 *   (no RTC — clock runs off millis(), defaults to 00:00 at boot)
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <math.h>

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

// ── Modes ─────────────────────────────────────────────────────
enum Mode { MODE_ANIMATION, MODE_CLOCK, MODE_POMODORO, MODE_TERMINAL, MODE_USAGE, MODE_WEATHER };
Mode currentMode = MODE_ANIMATION;

// ── Animation-mode sub-state ──────────────────────────────────
#define EYE_NORMAL 0
#define EYE_SQUISH 1
uint8_t  eyeView      = EYE_NORMAL;
bool     busy         = false;
bool     backlightOn  = true;
uint8_t  animSpeed    = 1;   // 1=slow(default) 2=normal 3=fast

bool     dynamicMode    = true;   // auto-cycles idle animations when true (on by default)
uint32_t nextIdleAnimAt = 0;

uint16_t animBgColor  = 0;   // background for eye/logo animations

// ── Clock (no RTC — tracked from millis(), defaults to 00:00 at boot
//    unless set with 't'; behaves as a stopwatch until then) ───────────
uint32_t clockBaseMillis   = 0;
uint16_t clockBaseMinutes  = 0;   // minutes-since-midnight at clockBaseMillis
// ── Idle cycle: ANIM → CLOCK → WEATHER → USAGE → ANIM (3-5 min each) ─
uint8_t  idlePhase    = 0;   // 0=anim 1=clock 2=weather 3=usage
uint32_t idlePhaseAt  = 0;   // millis when current phase started
uint32_t idlePhaseDur = 0;   // ms to hold this phase (randomised)

// ── Pomodoro timer (ticks in the background regardless of currentMode) ─
bool     pomodoroActive    = false;
bool     pomodoroOnBreak   = false;
uint32_t pomodoroPhaseStart = 0;
bool     pomodoroRinging    = false;
uint32_t pomoRingingFlashAt = 0;
bool     pomoRingingFlashOn = false;
uint32_t pomoWorkMs         = 25UL * 60000;
uint32_t pomoBreakMs        =  5UL * 60000;
uint32_t pomoStoppedAt      = 0;
uint8_t  pomoIdlePhase      = 0;   // idle tomato animation phase
uint8_t  pomoIdleStep       = 0;
uint32_t pomoIdleNextAt     = 0;

// ── Alarm ────────────────────────────────────────────────────────────
bool     alarmArmed    = false;
bool     alarmRinging  = false;
uint32_t alarmAtMillis = 0;
uint32_t alarmFlashAt  = 0;
bool     alarmFlashOn  = false;

// ── Timer ────────────────────────────────────────────────────────────
bool     timerActive     = false;
bool     timerRinging    = false;
uint32_t timerAtMillis   = 0;
uint32_t timerDurationMs = 0;
uint32_t timerFlashAt    = 0;
bool     timerFlashOn    = false;

// ── Cowsay ───────────────────────────────────────────────────────────
bool     cowsayActive    = false;


// ── Numeric input prompt (used by 't' set-clock, 'r' set-alarm, and
//    'y' set-timer, Clock-mode-only commands) ──────────────────────────
enum InputKind { INPUT_NONE, INPUT_CLOCK_SET, INPUT_ALARM_SET, INPUT_TIMER_SEC, INPUT_POMO_WORK, INPUT_POMO_BREAK };
InputKind inputKind = INPUT_NONE;
String    inputBuf  = "";

// ── Claude usage push ('U' command — silent, machine-to-device only;
//    deliberately NOT routed through InputKind/inputBuf above, since that
//    machinery draws a full-screen "enter a value" prompt meant for humans,
//    and this gets sent automatically every ~60s by the web app. Payload is
//    12 fixed-width digits: SS WW RRR TTTTT (session%, weekly%, minutes
//    until session reset, minutes until weekly reset) — no RTC on this
//    board, so reset countdowns are tracked locally via millis() from the
//    moment each push is received, same trick as the alarm/timer code ────
uint16_t idleIntervalSec = 8;   // gap between idle eye animations (web-configurable)
bool     collectingIdleInterval = false;
String   idleBuf            = "";
bool     collectingUsage     = false;
String   usageBuf            = "";
int8_t   usageSessionPct     = -1; // -1 = no data yet
int8_t   usageWeeklyPct      = -1;
uint16_t usageSessionResetMin = 0; // minutes remaining AT THE TIME OF RECEIPT
uint16_t usageWeeklyResetMin  = 0;
uint32_t usageReceivedMillis  = 0;

// ── Silent clock sync ('T' command) — same idea as 'U' above: the web app
//    sends this automatically right after connecting, and it must NOT flash
//    Clock mode or any prompt on screen, since the device should stay on
//    whatever it's currently showing (default Animation) while it syncs ──
bool   collectingClockSet = false;
String clockSetBuf        = "";

// ── Silent Pomodoro set+start ('P' command) — same idea as 'U'/'T': sets
//    durations and starts in one atomic step, no separate f/k/p dance and
//    no visible prompt overlay. 5 digits: MM SS B (focus min, focus sec,
//    break min) ──────────────────────────────────────────────────────
bool   collectingPomoStart = false;
String pomoStartBuf        = "";

// ── Weather Data State ────────────────────────────────────────
enum WeatherCondition { WC_CLEAR, WC_CLOUDY, WC_FOG, WC_RAIN, WC_STORM };
struct WeatherData {
  WeatherCondition cond;
  int8_t tempC;
  int8_t feelsC;
  uint8_t humidity;
  bool hasData;
};
WeatherData wx = { WC_CLEAR, 30, 32, 80, false };
String wxLoc = "Ho Chi Minh City";
String wxCondStr = "";

WeatherCondition wmoToCondition(uint8_t wmo) {
  if (wmo <= 1) return WC_CLEAR;
  if (wmo == 45 || wmo == 48) return WC_FOG;
  if (wmo >= 51 && wmo <= 82) return WC_RAIN;
  if (wmo == 95 || wmo == 96 || wmo == 99) return WC_STORM;
  return WC_CLOUDY;
}

// Weather collection state
bool wxCollecting = false;
String wxBuf = "";

// Weather animation globals
struct Drop {
  int8_t x, y;
} drops[30];

uint32_t wxLastFrame = 0;
uint16_t wxFrame = 0;

// Lightning state for STORM
bool     lightningActive = false;
uint16_t lightningFrame = 0;
int8_t   lightningX = 0;

// Cloud state for CLOUDY
int8_t   cloudX[2] = {5, 35};

// Clear state
int16_t  clearBirdX = -15;
int8_t   clearBirdY = 6;
int16_t  miniCloudX = 5;
int8_t   miniCloudY = 3;
int16_t  planeX = -40;
int8_t   planeY = 12;
int16_t  claudeX = 20;
int8_t   claudeDir = 1;

// Sprites
const uint16_t SUN_MASK[16] PROGMEM = {
  0b0000011111100000,
  0b0001111111110000,
  0b0011111111111100,
  0b0111111111111110,
  0b0111111111111110,
  0b1111111111111111,
  0b1111111111111111,
  0b1111111111111111,
  0b1111111111111111,
  0b1111111111111111,
  0b1111111111111111,
  0b0111111111111110,
  0b0111111111111110,
  0b0011111111111100,
  0b0001111111110000,
  0b0000011111100000
};

const uint16_t CLOUD_MASK_1[7] PROGMEM = {
  0b00001111000000,
  0b00111111110000,
  0b01111111111100,
  0b11111111111111,
  0b11111111111111,
  0b01111111111110,
  0b00011111110000
};

const uint16_t CLOUD_MASK_2[7] PROGMEM = {
  0b00000111110000,
  0b00011111111100,
  0b00111111111110,
  0b01111111111111,
  0b11111111111111,
  0b01111111111110,
  0b00011111110000
};

const uint8_t MINI_CLOUD_MASK[4] PROGMEM = {
  0b00111000,
  0b01111100,
  0b11111110,
  0b01111100
};

const int8_t LIGHTNING_BOLT[28][2] PROGMEM = {
  {0, 0}, {0, 1}, {1, 2}, {1, 3}, {0, 4},
  {-1, 5}, {-1, 6}, {0, 7}, {1, 8}, {1, 9},
  {0, 10}, {-1, 11}, {-2, 12}, {-2, 13}, {-1, 14},
  {0, 15}, {0, 16}, {1, 17}, {2, 18}, {2, 19},
  {1, 20}, {0, 21}, {-1, 22}, {-1, 23}, {0, 24},
  {1, 25}, {1, 26}, {0, 27}
};

const int8_t PULSE_LUT[16] PROGMEM = {0, 0, 1, 1, 1, 1, 0, 0, 0, 0, -1, -1, -1, -1, 0, 0};

// ── Terminal ──────────────────────────────────────────────────
#define TERM_COLS      15
#define TERM_ROWS       8
#define TERM_CHAR_W    12
#define TERM_CHAR_H    20
#define TERM_PAD_X      8
#define TERM_PAD_Y     18

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
//  EYES
// ═════════════════════════════════════════════════════════════

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
  char buf[4];

  uint16_t C_CARD = tft.color565(18, 22, 28);
  uint16_t C_DIM  = tft.color565(35, 33, 30);

  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 3, C_ORANGE);

  // ── Hour card (left) ─────────────────────
  tft.fillRoundRect(8, 42, 108, 84, 8, C_CARD);
  tft.setTextColor(C_WHITE); tft.setTextSize(6);
  snprintf(buf, sizeof(buf), "%02d", hh);
  tft.setCursor(26, 60);
  tft.print(buf);
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(53, 133);
  tft.print("HRS");

  // ── Minute card (right) ──────────────────
  tft.fillRoundRect(124, 42, 108, 84, 8, C_CARD);
  tft.setTextColor(C_WHITE); tft.setTextSize(6);
  snprintf(buf, sizeof(buf), "%02d", mm);
  tft.setCursor(142, 60);
  tft.print(buf);
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(166, 133);
  tft.print("MIN");

  // ── Colon dots ───────────────────────────
  tft.fillRect(118, 72, 6, 6, C_MUTED);
  tft.fillRect(118, 96, 6, 6, C_MUTED);

  // ── Workday dashed bar, red → green (9 AM → 6 PM) ───
  const uint16_t lunchStart = 12 * 60 + 30;
  const uint16_t lunchEnd   = 13 * 60 + 30;
  const uint16_t workStart  = 9  * 60;
  const uint16_t workEnd    = 18 * 60;

  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(8, 162);   tft.print("WORK");
  tft.setCursor(208, 162); tft.print("HOME");

  uint8_t activeDashes = 0;
  if (m >= workStart && m < workEnd)
    activeDashes = (uint32_t)(m - workStart) * 12 / (workEnd - workStart);
  else if (m >= workEnd)
    activeDashes = 12;

  for (uint8_t i = 0; i < 12; i++) {
    tft.fillRect(8 + i * 19, 174, 14, 6, i < activeDashes ? C_ORANGE : C_DIM);
  }

  // ── Single text line: random from message pool ───────────────
  static const char* encMsgs[] = {
    "let's get it!", "keep grinding!", "locked in.",
    "you got this!", "stay focused.", "make it count.",
    "no days off!", "heads down.", "push through!",
    "eyes on the prize", "one step at a time", "almost there!",
    "past halfway!", "keep moving.", "don't stop now!"
  };
  static const char* eggMsgs[] = {
    "go touch grass", "drink water! >_<", "stretch break?",
    "still here huh", "blink twice if ok", "have you eaten?",
    "you look tired...", "vla go brrr", "robot uprising soon",
    "404: chill not found", "skill issue?", "based.",
    "no cap fr fr", "touch some grass bro", "we so back"
  };

  char statusBuf[22];
  if (m < workStart) {
    uint16_t left = workStart - m;
    snprintf(statusBuf, sizeof(statusBuf), "starts in %dh %dm", left / 60, left % 60);
  } else if (m >= workEnd) {
    snprintf(statusBuf, sizeof(statusBuf), "done for today");
  } else if (m >= lunchStart && m < lunchEnd) {
    snprintf(statusBuf, sizeof(statusBuf), "back in %dm", lunchEnd - m);
  } else {
    uint16_t left = workEnd - m;
    snprintf(statusBuf, sizeof(statusBuf), "%dh %dm left", left / 60, left % 60);
  }

  const char* lineText;

  if (m >= lunchStart && m < lunchEnd && random(2) == 0) {
    lineText = "enjoy lunch! :)";
  } else {
    uint8_t roll = random(10);
    if      (roll < 4) { lineText = statusBuf;           }
    else if (roll < 9) { lineText = encMsgs[random(15)]; }
    else               { lineText = eggMsgs[random(15)]; }
  }

  tft.setTextColor(C_MUTED); tft.setTextSize(2);
  tft.setCursor(DISP_W / 2 - strlen(lineText) * 6, 205);
  tft.print(lineText);
}

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

#define PX 4

void fillPx(int16_t x, int16_t y, uint16_t col) {
  if (x >= 0 && x < 60 && y >= 0 && y < 30) {
    tft.fillRect(x * PX, y * PX, PX, PX, col);
  }
}

void drawSunBody(int8_t sx, int8_t sy, uint16_t col) {
  for (int8_t r = 0; r < 16; r++) {
    uint16_t rowVal = pgm_read_word(&SUN_MASK[r]);
    for (int8_t c = 0; c < 16; c++) {
      if ((rowVal >> (15 - c)) & 1) {
        fillPx(sx + c, sy + r, col);
      }
    }
  }
}

void drawSunRays(int8_t sx, int8_t sy, int8_t len, uint16_t col) {
  int8_t cx = sx + 8;
  int8_t cy = sy + 8;
  for (int8_t i = 0; i < len; i++) {
    fillPx(cx, cy - 9 - i, col);         // N
    fillPx(cx, cy + 9 + i, col);         // S
    fillPx(cx - 9 - i, cy, col);         // W
    fillPx(cx + 9 + i, cy, col);         // E
    fillPx(cx - 7 - i, cy - 7 - i, col); // NW
    fillPx(cx + 7 - i, cy - 7 - i, col); // NE
    fillPx(cx - 7 - i, cy + 7 + i, col); // SW
    fillPx(cx + 7 - i, cy + 7 + i, col); // SE
  }
}

void drawCloud(int8_t cx, int8_t cy, const uint16_t* mask, uint16_t col) {
  for (int8_t r = 0; r < 7; r++) {
    uint16_t rowVal = pgm_read_word(&mask[r]);
    for (int8_t c = 0; c < 14; c++) {
      if ((rowVal >> (13 - c)) & 1) {
        fillPx(cx + c, cy + r, col);
      }
    }
  }
}

void drawMiniCloud(int8_t cx, int8_t cy, uint16_t col) {
  for (int8_t r = 0; r < 4; r++) {
    uint8_t rowVal = pgm_read_byte(&MINI_CLOUD_MASK[r]);
    for (int8_t c = 0; c < 8; c++) {
      if ((rowVal >> (7 - c)) & 1) {
        fillPx(cx + c, cy + r, col);
      }
    }
  }
}

void drawMiniClaude(int8_t cx, int8_t cy, int8_t dir, uint16_t bodyCol, uint16_t eyeCol) {
  // Ears
  fillPx(cx, cy, bodyCol);
  fillPx(cx + 4, cy, bodyCol);
  // Head top
  for (int8_t c = 0; c < 5; c++) {
    fillPx(cx + c, cy + 1, bodyCol);
  }
  // Eyes
  fillPx(cx, cy + 2, bodyCol);
  fillPx(cx + 4, cy + 2, bodyCol);
  if (dir == 1) {
    fillPx(cx + 1, cy + 2, bodyCol);
    fillPx(cx + 2, cy + 2, eyeCol);
    fillPx(cx + 3, cy + 2, eyeCol);
  } else {
    fillPx(cx + 1, cy + 2, eyeCol);
    fillPx(cx + 2, cy + 2, eyeCol);
    fillPx(cx + 3, cy + 2, bodyCol);
  }
  // Chin
  fillPx(cx + 1, cy + 3, bodyCol);
  fillPx(cx + 2, cy + 3, bodyCol);
  fillPx(cx + 3, cy + 3, bodyCol);
}

void drawLightning(int8_t sx, uint16_t col) {
  for (uint8_t i = 0; i < 28; i++) {
    int8_t dx = pgm_read_byte(&LIGHTNING_BOLT[i][0]);
    int8_t dy = pgm_read_byte(&LIGHTNING_BOLT[i][1]);
    fillPx(sx + dx, 2 + dy, col);
    fillPx(sx + dx + 1, 2 + dy, col);
  }
}

void initWeatherAnimation() {
  wxFrame = 0;
  wxLastFrame = millis();
  lightningActive = false;
  
  clearBirdX = -15;
  clearBirdY = 6;
  miniCloudX = 5;
  miniCloudY = 3;
  planeX = -40;
  planeY = 12;
  claudeX = 20;
  claudeDir = 1;
  
  if (wx.hasData) {
    if (wx.cond == WC_RAIN || wx.cond == WC_STORM) {
      for (uint8_t i = 0; i < 30; i++) {
        drops[i].x = random(0, 60);
        drops[i].y = random(-10, 30);
      }
    } else if (wx.cond == WC_CLOUDY) {
      cloudX[0] = 5;
      cloudX[1] = 35;
    }
  }
}

void drawWeatherView() {
  uint16_t skyCol = C_DARKBG;
  const char* condStr = "UNKNOWN";
  uint16_t accentCol = C_WHITE;
  
  if (wx.hasData) {
    switch (wx.cond) {
      case WC_CLEAR:
        skyCol = tft.color565(100, 180, 255);
        condStr = "CLEAR";
        accentCol = tft.color565(255, 220, 100);
        break;
      case WC_CLOUDY:
        skyCol = tft.color565(100, 120, 145);
        condStr = "CLOUDY";
        accentCol = tft.color565(200, 220, 240);
        break;
      case WC_FOG:
        skyCol = tft.color565(170, 170, 160);
        condStr = "FOGGY";
        accentCol = tft.color565(220, 220, 220);
        break;
      case WC_RAIN:
        skyCol = tft.color565(20, 30, 55);
        condStr = "RAINY";
        accentCol = tft.color565(100, 200, 255);
        break;
      case WC_STORM:
        skyCol = tft.color565(10, 15, 25);
        condStr = "STORM";
        accentCol = tft.color565(255, 255, 150);
        break;
    }
  } else {
    skyCol = C_DARKBG;
    condStr = "NO DATA";
    accentCol = C_MUTED;
  }
  
  // Fill the weather scene zone (y=0 to 120, i.e., 30 logical px)
  tft.fillRect(0, 0, DISP_W, 120, skyCol);
  
  // Clear the info zone below (y=120 to 240)
  tft.fillRect(0, 120, DISP_W, DISP_H - 120, C_DARKBG);
  
  tft.setTextColor(C_WHITE);
  
  if (wx.hasData) {
    // Temperature
    tft.setTextSize(5);
    tft.setCursor(12, 125);
    tft.print(wx.tempC);
    tft.print("C");
    
    // Feels-like and humidity
    tft.setTextSize(2);
    tft.setCursor(12, 165);
    tft.print("Feels ");
    tft.print(wx.feelsC);
    tft.print("C  ");
    tft.print(wx.humidity);
    tft.print("%");
    
    // Condition label
    tft.setTextSize(2);
    tft.setTextColor(accentCol);
    tft.setCursor(12, 185);
    if (wxCondStr.length() > 0) {
      tft.print(wxCondStr);
    } else {
      tft.print(condStr);
    }
    
    // Location label
    tft.setTextSize(2);
    tft.setTextColor(C_MUTED);
    tft.setCursor(12, 205);
    tft.print(wxLoc);
  } else {
    tft.setTextSize(5);
    tft.setCursor(12, 125);
    tft.print("--C");
    
    tft.setTextSize(2);
    tft.setCursor(12, 165);
    tft.setTextColor(C_MUTED);
    tft.print("Feels --C  --%");
    
    tft.setTextSize(2);
    tft.setTextColor(accentCol);
    tft.setCursor(12, 185);
    tft.print(condStr);
    
    // Location label
    tft.setTextSize(2);
    tft.setTextColor(C_MUTED);
    tft.setCursor(12, 205);
    tft.print(wxLoc);
  }
  
  initWeatherAnimation();
  
  // If clear/cloudy, draw initial static elements
  if (wx.hasData) {
    if (wx.cond == WC_CLEAR) {
      tft.fillRect(0, 29 * PX, DISP_W, PX, tft.color565(34, 139, 34)); // draw grass
      drawSunBody(42, 2, tft.color565(255, 215, 0));
      drawSunRays(42, 2, 4, tft.color565(255, 215, 0));
    } else if (wx.cond == WC_CLOUDY) {
      drawCloud(cloudX[0], 6, CLOUD_MASK_1, C_WHITE);
      drawCloud(cloudX[1], 14, CLOUD_MASK_2, C_WHITE);
    }
  }
}

void updateStormRainAnimation(uint8_t speed, uint16_t skyCol, uint16_t dropCol) {
  for (uint8_t i = 0; i < 30; i++) {
    for (int8_t dy = 0; dy < 3; dy++) {
      fillPx(drops[i].x - dy, drops[i].y + dy, skyCol);
    }
    
    drops[i].y += speed;
    drops[i].x -= 1;
    
    if (drops[i].y >= 30 || drops[i].x < -2) {
      drops[i].y = -2;
      drops[i].x = random(5, 65);
    }
    
    for (int8_t dy = 0; dy < 3; dy++) {
      fillPx(drops[i].x - dy, drops[i].y + dy, dropCol);
    }
  }
}

void updateRainAnimation(uint8_t speed, uint16_t skyCol, uint16_t dropCol) {
  for (uint8_t i = 0; i < 30; i++) {
    // Erase old drop (1x3 strip)
    for (int8_t dy = 0; dy < 3; dy++) {
      fillPx(drops[i].x, drops[i].y + dy, skyCol);
    }
    
    // Advance drop
    drops[i].y += speed;
    
    // Wrap if off screen
    if (drops[i].y >= 30) {
      drops[i].y = -2;
      drops[i].x = random(0, 60);
    }
    
    // Draw new drop (1x3 strip)
    for (int8_t dy = 0; dy < 3; dy++) {
      fillPx(drops[i].x, drops[i].y + dy, dropCol);
    }
  }
}

void updateWeatherView() {
  if (millis() - wxLastFrame < 100) return;
  wxLastFrame = millis();
  wxFrame++;
  
  if (!wx.hasData) return;
  
  uint16_t skyCol = C_DARKBG;
  switch (wx.cond) {
    case WC_CLEAR:  skyCol = tft.color565(100, 180, 255); break;
    case WC_CLOUDY: skyCol = tft.color565(100, 120, 145); break;
    case WC_FOG:    skyCol = tft.color565(170, 170, 160); break;
    case WC_RAIN:   skyCol = tft.color565(20, 30, 55);    break;
    case WC_STORM:  skyCol = tft.color565(10, 15, 25);    break;
  }
  
  if (wx.cond == WC_RAIN) {
    updateRainAnimation(2, skyCol, tft.color565(100, 200, 255));
  }
  else if (wx.cond == WC_STORM) {
    updateStormRainAnimation(3, skyCol, tft.color565(150, 220, 255));
    
    if (lightningActive) {
      if (wxFrame == lightningFrame + 1) {
        // Frame 1: Return sky to normal, redraw slanted drops, and draw visible lightning bolt
        tft.fillRect(0, 0, DISP_W, 120, skyCol);
        for (uint8_t i = 0; i < 30; i++) {
          for (int8_t dy = 0; dy < 3; dy++) {
            fillPx(drops[i].x - dy, drops[i].y + dy, tft.color565(150, 220, 255));
          }
        }
        drawLightning(lightningX, tft.color565(255, 255, 150));
      }
      else if (wxFrame >= lightningFrame + 2 && wxFrame <= lightningFrame + 4) {
        // Frame 2, 3, 4: Keep lightning bolt visible (redraw to sit on top of rain drops)
        drawLightning(lightningX, tft.color565(255, 255, 150));
      }
      else if (wxFrame == lightningFrame + 5) {
        // Frame 5: Erase lightning bolt
        drawLightning(lightningX, skyCol);
        lightningActive = false;
      }
    }
    
    if (!lightningActive && random(0, 70) == 0) {
      lightningActive = true;
      lightningFrame = wxFrame;
      lightningX = random(5, 50);
      
      // Flash sky white (exactly 1 frame)
      tft.fillRect(0, 0, DISP_W, 120, C_WHITE);
    }
  }
  else if (wx.cond == WC_CLEAR) {
    uint16_t sunCol = tft.color565(255, 215, 0);
    uint16_t flowerCol = tft.color565(255, 60, 60);
    uint16_t stemCol = tft.color565(34, 100, 34);
    uint16_t birdCol = tft.color565(40, 70, 120);
    uint16_t planeCol = tft.color565(180, 180, 180);
    uint16_t trunkCol = tft.color565(139, 69, 19);
    uint16_t leafCol = tft.color565(34, 139, 34);
    uint16_t claudeBody = tft.color565(255, 180, 100);
    
    // 1. Erase old bird
    if (clearBirdX >= -3 && clearBirdX < 60) {
      fillPx(clearBirdX, clearBirdY, skyCol);
      fillPx(clearBirdX - 1, clearBirdY - 1, skyCol);
      fillPx(clearBirdX + 1, clearBirdY - 1, skyCol);
      fillPx(clearBirdX - 1, clearBirdY + 1, skyCol);
      fillPx(clearBirdX + 1, clearBirdY + 1, skyCol);
    }
    
    // 2. Erase old plane
    if (planeX >= 0 && planeX < 65) {
      fillPx(planeX, planeY, skyCol);
      fillPx(planeX - 1, planeY, skyCol);
      fillPx(planeX - 2, planeY, skyCol);
      fillPx(planeX - 3, planeY, skyCol);
      fillPx(planeX - 1, planeY - 1, skyCol);
      fillPx(planeX - 1, planeY + 1, skyCol);
      fillPx(planeX - 3, planeY - 1, skyCol);
    }
    
    // 3. Erase old mini cloud
    if (miniCloudX >= -8 && miniCloudX < 60) {
      drawMiniCloud(miniCloudX, miniCloudY, skyCol);
    }
    
    // 4. Erase old mini Claude
    int8_t claudeBob = (claudeX % 2 == 0) ? 0 : 1;
    int8_t claudeY = 25 - claudeBob;
    for (int8_t r = 0; r < 4; r++) {
      for (int8_t c = 0; c < 5; c++) {
        fillPx(claudeX + c, claudeY + r, skyCol);
      }
    }
    
    // 5. Update positions
    clearBirdX++;
    if (clearBirdX >= 60) {
      clearBirdX = -20;
      clearBirdY = random(3, 10);
    }
    
    if (wxFrame % 2 == 0) {
      planeX++;
      if (planeX >= 80) {
        planeX = -80;
        planeY = random(4, 15);
      }
    }
    
    // Cloud drifts 1px every frame for smoothness and speed
    miniCloudX++;
    if (miniCloudX >= 60) {
      miniCloudX = -10;
      miniCloudY = random(2, 13); // random height!
    }
    
    // Mini Claude walks back and forth
    if (wxFrame % 3 == 0) {
      claudeX += claudeDir;
      if (claudeX >= 50) {
        claudeDir = -1;
      } else if (claudeX <= 15) {
        claudeDir = 1;
      }
    }
    
    // 6. Draw sun (at side sx=42, sy=2)
    drawSunRays(42, 2, 5, skyCol);
    drawSunBody(42, 2, sunCol);
    int8_t rayLen = 4 + (int8_t)pgm_read_byte(&PULSE_LUT[wxFrame % 16]);
    drawSunRays(42, 2, rayLen, sunCol);
    
    // 7. Draw mini cloud
    if (miniCloudX >= -8 && miniCloudX < 60) {
      drawMiniCloud(miniCloudX, miniCloudY, C_WHITE);
    }
    
    // 8. Draw flying plane
    if (planeX >= 0 && planeX < 60) {
      fillPx(planeX, planeY, planeCol);
      fillPx(planeX - 1, planeY, planeCol);
      fillPx(planeX - 2, planeY, planeCol);
      fillPx(planeX - 3, planeY, planeCol);
      fillPx(planeX - 1, planeY - 1, planeCol);
      fillPx(planeX - 1, planeY + 1, planeCol);
      fillPx(planeX - 3, planeY - 1, planeCol);
    }
    
    // 9. Draw flying bird
    if (clearBirdX >= -3 && clearBirdX < 60) {
      fillPx(clearBirdX, clearBirdY, birdCol);
      if (wxFrame % 6 < 3) {
        fillPx(clearBirdX - 1, clearBirdY - 1, birdCol);
        fillPx(clearBirdX + 1, clearBirdY - 1, birdCol);
      } else {
        fillPx(clearBirdX - 1, clearBirdY + 1, birdCol);
        fillPx(clearBirdX + 1, clearBirdY + 1, birdCol);
      }
    }
    
    // 10. Draw swaying flowers
    int8_t sway = (wxFrame % 12 < 6) ? 0 : 1;
    int8_t flowerX[2] = {25, 48};
    for (uint8_t i = 0; i < 2; i++) {
      fillPx(flowerX[i], 27, skyCol);
      fillPx(flowerX[i] + 1, 27, skyCol);
      fillPx(flowerX[i] - 1, 27, skyCol);
      
      fillPx(flowerX[i], 28, stemCol);
      fillPx(flowerX[i] + sway, 27, flowerCol);
    }
    
    // 11. Draw mini Claude (bobbing walk)
    int8_t drawClaudeBob = (claudeX % 2 == 0) ? 0 : 1;
    int8_t drawClaudeY = 25 - drawClaudeBob;
    drawMiniClaude(claudeX, drawClaudeY, claudeDir, claudeBody, C_DARKBG);
    
    // 12. Draw small tree on the left (x=6)
    fillPx(6, 27, trunkCol);
    fillPx(6, 28, trunkCol);
    fillPx(6, 23, leafCol);
    
    fillPx(5, 24, leafCol);
    fillPx(6, 24, leafCol);
    fillPx(7, 24, leafCol);
    
    fillPx(4, 25, leafCol);
    fillPx(5, 25, leafCol);
    fillPx(6, 25, leafCol);
    fillPx(7, 25, leafCol);
    fillPx(8, 25, leafCol);
    
    fillPx(5, 26, leafCol);
    fillPx(6, 26, leafCol);
    fillPx(7, 26, leafCol);
  }
  else if (wx.cond == WC_CLOUDY) {
    if (wxFrame % 3 == 0) {
      for (uint8_t i = 0; i < 2; i++) {
        drawCloud(cloudX[i], 6 + i * 8, i == 0 ? CLOUD_MASK_1 : CLOUD_MASK_2, skyCol);
        cloudX[i]++;
        if (cloudX[i] >= 65) cloudX[i] = -14;
        drawCloud(cloudX[i], 6 + i * 8, i == 0 ? CLOUD_MASK_1 : CLOUD_MASK_2, C_WHITE);
      }
    }
  }
  else if (wx.cond == WC_FOG) {
    int8_t shift = (wxFrame / 2) % 60;
    int8_t bands[4] = {4, 11, 18, 25};
    for (uint8_t b = 0; b < 4; b++) {
      int8_t by = bands[b];
      for (int8_t y = by; y < by + 2; y++) {
        for (int8_t x = 0; x < 60; x++) {
          bool isWhite = ((x - shift) ^ y) & 1;
          uint16_t col = isWhite ? tft.color565(240, 240, 235) : tft.color565(190, 190, 185);
          fillPx(x, y, col);
        }
      }
    }
  }
}

// Keep Clock mode ticking forward while it's the active mode
void updateClockViewIfShown() {
  if (currentMode != MODE_CLOCK) return;

  if (timerActive) {
    static uint32_t lastTimerSec = 0xFFFF;
    uint32_t remainSec = (timerAtMillis > millis() ? timerAtMillis - millis() : 0) / 1000;
    if (remainSec != lastTimerSec) {
      lastTimerSec = remainSec;
      uint8_t mm = remainSec / 60;
      uint8_t ss = remainSec % 60;
      drawTimerTime(mm, ss);
    }
    return;
  }

  static uint16_t lastMin = 0xFFFF;
  uint16_t m = clockNowMinutes();
  if (m != lastMin) { lastMin = m; drawClockView(); }
}

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
    clockBaseMinutes = hh * 60 + mm;
    clockBaseMillis  = millis();
    Serial.print("Clock set to "); Serial.println(inputBuf);
  } else if (inputKind == INPUT_ALARM_SET) {
    int val = inputBuf.toInt();
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
  } else if (inputKind == INPUT_TIMER_SEC) {
    uint32_t secs = inputBuf.toInt();
    if (secs > 0) {
      armTimer(secs);
      Serial.print("Timer set to "); Serial.print(secs); Serial.println(" seconds");
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

    case 3: {  // ── Splat ────────────────────────────
      if (pomoIdleStep == 0) {
        tft.fillScreen(C_DARKBG);
        tft.fillRect(0, 0, DISP_W, 3, C_ORANGE);

        // Dim ghost of the text
        tft.setTextColor(DIM); tft.setTextSize(5);
        tft.setCursor(TX, TY);
        tft.print("POMODORO");

        // Main splat blob (hits left of text)
        int16_t cx = 56, cy = 112;
        tft.fillCircle(cx, cy, 38, RED);
        tft.fillCircle(cx + 6, cy - 8, 20, RED2);

        // Splatter drops — fixed positions for consistency
        const int16_t DX[] = {-48,-38, 30, 68, 90,120, 48,-20,150, 10, 80,-10};
        const int16_t DY[] = {-22, 28,-42,-18, 20, -8, 38, 50, 30,-58,-44,-48};
        const uint8_t DR[] = {  7,  5,  6,  8,  5,  6,  5,  6,  4,  5,  4,  4};
        for (uint8_t i = 0; i < 12; i++)
          tft.fillCircle(cx + DX[i], cy + DY[i], DR[i], RED);

        // Pixel-art seeds in the blob
        tft.fillRect(cx - 8,  cy - 6,  6, 10, DIM);
        tft.fillRect(cx + 10, cy + 4,  6, 10, DIM);
        tft.fillRect(cx - 2,  cy + 12, 6, 10, DIM);
      }
      pomoIdleStep++;
      pomoIdleNextAt = now + 50;
      if (pomoIdleStep >= 50) { pomoIdlePhase = 0; pomoIdleStep = 0; }  // 2.5s hold then loop
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
  if (millis() - pomoRingingFlashAt < 300) return;
  pomoRingingFlashAt = millis();
  pomoRingingFlashOn = !pomoRingingFlashOn;
  
  uint16_t bg = pomoRingingFlashOn ? C_ORANGE : C_WHITE;
  uint16_t fg = pomoRingingFlashOn ? C_WHITE  : C_ORANGE;
  tft.fillScreen(bg);
  tft.setTextColor(fg); tft.setTextSize(3);
  tft.setCursor(30, DISP_H / 2 - 12);
  if (pomodoroOnBreak) {
    tft.print("BREAK!");
  } else {
    tft.print("FOCUS!");
  }
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

// ═════════════════════════════════════════════════════════════
//  ALARM (overlays on top of whatever mode is active)
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
  switchMode(currentMode);  // redraw whatever was on screen before it rang
}

void disarmAlarm() {
  alarmArmed   = false;
  alarmRinging = false;
}

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
}

void checkTimer() {
  if (timerActive && millis() >= timerAtMillis) {
    timerActive  = false;
    timerRinging = true;
    timerFlashAt = 0;
  }
}

void updateTimerFlash() {
  if (!timerRinging) return;
  if (millis() - timerFlashAt < 300) return;
  timerFlashAt = millis();
  timerFlashOn = !timerFlashOn;
  uint16_t bg = timerFlashOn ? C_ORANGE : C_WHITE;
  uint16_t fg = timerFlashOn ? C_WHITE  : C_ORANGE;
  tft.fillScreen(bg);
  tft.setTextColor(fg); tft.setTextSize(3);
  tft.setCursor(30, DISP_H / 2 - 12);
  tft.print("TIMER!");
}

void dismissTimer() {
  timerRinging = false;
  switchMode(currentMode);
}

void drawTimerStatic() {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_GREEN);
  tft.setTextColor(C_MUTED); tft.setTextSize(2);
  tft.setCursor(DISP_W / 2 - 30, DISP_H / 2 + 30);
  tft.print("TIMER");
}

void drawTimerTime(uint8_t mm, uint8_t ss) {
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", mm, ss);
  // Erase only the time text bounding box to prevent screen jitter
  tft.fillRect(0, DISP_H / 2 - 35, DISP_W, 55, C_DARKBG);
  tft.setTextColor(C_WHITE); tft.setTextSize(6);
  tft.setCursor(DISP_W / 2 - 72, DISP_H / 2 - 24);
  tft.print(buf);
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
};
const uint8_t IDLE_ANIM_COUNT = sizeof(IDLE_ANIMS) / sizeof(IDLE_ANIMS[0]);

void playRandomIdleAnim() {
  IDLE_ANIMS[random(IDLE_ANIM_COUNT)]();
  uint32_t base = idleIntervalSec * 1000UL;
  nextIdleAnimAt = millis() + random(base * 3 / 4, base * 5 / 4);
}


// ═════════════════════════════════════════════════════════════
//  MODE SWITCHING
// ═════════════════════════════════════════════════════════════

// Single source of truth for "draw whatever the current mode looks like" —
// used both for actual mode switches and for redraws after an overlay
// (alarm / numeric input) closes.
void switchMode(Mode m) {
  currentMode = m;
  switch (m) {
    case MODE_ANIMATION:
      if (eyeView == EYE_SQUISH) drawSquishEyes(); else drawNormalEyes();
      Serial.println("MODE:ANIMATION");
      break;
    case MODE_CLOCK:
      if (timerActive) {
        drawTimerStatic();
        uint32_t remainSec = (timerAtMillis > millis() ? timerAtMillis - millis() : 0) / 1000;
        drawTimerTime(remainSec / 60, remainSec % 60);
      } else {
        drawClockView();
      }
      Serial.println("MODE:CLOCK");
      break;
    case MODE_POMODORO:
      if (pomodoroActive) {
        drawPomodoroStatic();
        uint8_t mm, ss; pomodoroRemaining(mm, ss);
        drawPomodoroTime(mm, ss);
      } else {
        drawPomodoroIdle();
      }
      // Re-announce truth every time this mode is (re)entered, so a web
      // client that just connected/reconnected resyncs before it acts.
      Serial.println(pomodoroActive ? "POMO:ON" : "POMO:OFF");
      Serial.println("MODE:POMODORO");
      break;
    case MODE_TERMINAL:
      termClear();
      termFullRedraw();
      Serial.println("MODE:TERMINAL");
      break;
    case MODE_USAGE:
      drawUsageView();
      Serial.println("MODE:USAGE");
      break;
    case MODE_WEATHER:
      drawWeatherView();
      Serial.println("WEATHER");
      break;
  }
}

void handleAnimationKey(char c) {
  switch (c) {
    case 'w': eyeView = EYE_NORMAL; animNormalEyes(); break;
    case 's': eyeView = EYE_SQUISH; animSquishEyes(); break;
    case 'z': eyeView = EYE_NORMAL; animLogoReveal(); break;
    case 'e': eyeView = EYE_NORMAL; animBlink(); break;
    case 'f': eyeView = EYE_NORMAL; animDoubleBlink(); break;
    case 'g': eyeView = EYE_NORMAL; animLookAround(); break;
    case 'h': eyeView = EYE_NORMAL; animWink(); break;
    case 'i': eyeView = EYE_NORMAL; animSleepy(); break;
    case 'j': eyeView = EYE_NORMAL; animSurprised(); break;
    case 'k': eyeView = EYE_NORMAL; animSquint(); break;
    case 'l': eyeView = EYE_NORMAL; animNod(); break;
    case 'n': eyeView = EYE_NORMAL; animShake(); break;
    case 'o': eyeView = EYE_NORMAL; animRoll(); break;
    case 'u': eyeView = EYE_NORMAL; animCrossEyed(); break;
    case 'v': eyeView = EYE_NORMAL; animTiltConfused(); break;
    case 'x': eyeView = EYE_NORMAL; animExcited(); break;
    case 'p': eyeView = EYE_NORMAL; animHeartEyes(); break;
    case 'q': eyeView = EYE_NORMAL; animDizzyEyes(); break;
    case 'a': eyeView = EYE_NORMAL; animGlitchEyes(); break;
    default: break;
  }
}

void handleClockKey(char c) {
  switch (c) {
    case 't': enterTimeInput(INPUT_CLOCK_SET); break;
    case 'r': enterTimeInput(INPUT_ALARM_SET); break;
    case 'y': enterTimeInput(INPUT_TIMER_SEC); break;
    case 'q': disarmTimer(); switchMode(currentMode); break;
    case 'a': disarmAlarm(); switchMode(currentMode); break;
    default: break;
  }
}

void handlePomodoroKey(char c) {
  if (c == 'p') {
    pomodoroActive = !pomodoroActive;
    if (pomodoroActive) {
      pomodoroOnBreak    = false;
      pomodoroRinging    = false;
      pomodoroPhaseStart = millis();
      drawPomodoroStatic();
      drawPomodoroTime(pomoWorkMs / 60000UL, 0);
    } else {
      drawPomodoroIdle();
      pomoStoppedAt = millis();
    }
    // Echo the real state — the web app trusts this instead of guessing,
    // since pomodoroActive persists on the device across page reloads/
    // reconnects and a stale local guess would invert the next press.
    Serial.println(pomodoroActive ? "POMO:ON" : "POMO:OFF");
  } else if (c == 'f') {
    enterTimeInput(INPUT_POMO_WORK);
  } else if (c == 'k') {
    enterTimeInput(INPUT_POMO_BREAK);
  }
}

// Single source of truth for command dispatch — shared by Serial input
// the single source of truth for all control input.
//
// Priority: alarm overlay > numeric input overlay > terminal free-text >
// global keys (mode switch, backlight, speed) > current mode's own keys.
void handleChar(char c) {
  if (cowsayActive) {
    cowsayActive = false;
    switchMode(currentMode);
    return;
  }

  if (alarmRinging) { dismissAlarm(); return; }
  if (timerRinging) { dismissTimer(); return; }
  if (pomodoroActive && pomodoroRinging) {
    pomodoroRinging = false;
    pomodoroPhaseStart = millis();
    switchMode(currentMode);
    return;
  }

  if (inputKind != INPUT_NONE) { handleInputChar(c); return; }

  if (collectingIdleInterval) {
    if (c == '\n' || c == '\r') {
      int v = idleBuf.toInt();
      if (v >= 1 && v <= 120) idleIntervalSec = v;
      collectingIdleInterval = false;
      idleBuf = "";
    } else if (c >= '0' && c <= '9') {
      idleBuf += c;
    }
    return;
  }

  if (wxCollecting) {
    if (c == '\n' || c == '\r') {
      int comma1 = wxBuf.indexOf(',');
      int comma2 = wxBuf.indexOf(',', comma1 + 1);
      int comma3 = wxBuf.indexOf(',', comma2 + 1);
      if (comma1 != -1 && comma2 != -1 && comma3 != -1) {
        uint8_t wmo = wxBuf.substring(0, comma1).toInt();
        int8_t temp = wxBuf.substring(comma1 + 1, comma2).toInt();
        int8_t feels = wxBuf.substring(comma2 + 1, comma3).toInt();
        
        int comma4 = wxBuf.indexOf(',', comma3 + 1);
        uint8_t hum;
        if (comma4 != -1) {
          hum = wxBuf.substring(comma3 + 1, comma4).toInt();
          int comma5 = wxBuf.indexOf(',', comma4 + 1);
          if (comma5 != -1) {
            wxLoc = wxBuf.substring(comma4 + 1, comma5);
            wxCondStr = wxBuf.substring(comma5 + 1);
          } else {
            wxLoc = wxBuf.substring(comma4 + 1);
            wxCondStr = "";
          }
          wxLoc.trim();
          if (wxLoc.length() > 20) {
            wxLoc = wxLoc.substring(0, 20);
          }
          wxCondStr.trim();
          if (wxCondStr.length() > 20) {
            wxCondStr = wxCondStr.substring(0, 20);
          }
          if (wxLoc.length() == 0 || wxLoc == "HCMC") {
            wxLoc = "Ho Chi Minh City";
          }
        } else {
          hum = wxBuf.substring(comma3 + 1).toInt();
          wxCondStr = "";
          if (wxLoc.length() == 0 || wxLoc == "HCMC") {
            wxLoc = "Ho Chi Minh City";
          }
        }
        
        wx.cond = wmoToCondition(wmo);
        wx.tempC = temp;
        wx.feelsC = feels;
        wx.humidity = hum;
        wx.hasData = true;
        
        if (currentMode == MODE_WEATHER) {
          drawWeatherView();
        }
      }
      wxCollecting = false;
      wxBuf = "";
    } else if (c >= 32 && c <= 126 && wxBuf.length() < 64) {
      wxBuf += c;
    }
    return;
  }

  if (collectingUsage) {
    if (c == '\n' || c == '\r') {
      if (usageBuf.length() == 12) { // SS WW RRR TTTTT
        usageSessionPct      = usageBuf.substring(0, 2).toInt();
        usageWeeklyPct       = usageBuf.substring(2, 4).toInt();
        usageSessionResetMin = usageBuf.substring(4, 7).toInt();
        usageWeeklyResetMin  = usageBuf.substring(7, 12).toInt();
        usageReceivedMillis  = millis();
        if (currentMode == MODE_USAGE) drawUsageView();
      }
      collectingUsage = false;
      usageBuf = "";
    } else if (c >= '0' && c <= '9' && usageBuf.length() < 12) {
      usageBuf += c;
    }
    return;
  }

  if (collectingClockSet) {
    if (c == '\n' || c == '\r') {
      if (clockSetBuf.length() == 4) {
        int val = clockSetBuf.toInt();
        uint8_t hh = (val / 100) % 24;
        uint8_t mm = (val % 100) > 59 ? 59 : (val % 100);
        clockBaseMinutes = hh * 60 + mm;
        clockBaseMillis  = millis();
        if (currentMode == MODE_CLOCK) drawClockView(); // keep it accurate if already shown
      }
      collectingClockSet = false;
      clockSetBuf = "";
    } else if (c >= '0' && c <= '9' && clockSetBuf.length() < 4) {
      clockSetBuf += c;
    }
    return;
  }

  if (collectingPomoStart) {
    if (c == '\n' || c == '\r') {
      if (pomoStartBuf.length() == 5) { // MM SS B
        uint16_t fm = pomoStartBuf.substring(0, 2).toInt();
        uint8_t  fs = pomoStartBuf.substring(2, 4).toInt();
        uint8_t  bm = pomoStartBuf.substring(4, 5).toInt();
        pomoWorkMs         = (uint32_t)(fm * 60 + fs) * 1000UL;
        pomoBreakMs        = (uint32_t)bm * 60000UL;
        pomodoroActive     = true;
        pomodoroOnBreak    = false;
        pomodoroRinging    = false;
        pomodoroPhaseStart = millis();
        if (currentMode == MODE_POMODORO) {
          drawPomodoroStatic();
          drawPomodoroTime(fm, fs);
        }
        Serial.println("POMO:ON");
      }
      collectingPomoStart = false;
      pomoStartBuf = "";
    } else if (c >= '0' && c <= '9' && pomoStartBuf.length() < 5) {
      pomoStartBuf += c;
    }
    return;
  }

  if (currentMode == MODE_TERMINAL) {
    if (c == '\n' || c == '\r') {
      String cmdLine = termLines[termRow];
      cmdLine.trim();
      String lowerCmd = cmdLine;
      lowerCmd.toLowerCase();
      
      if (lowerCmd.equalsIgnoreCase("exit")) {
        switchMode(MODE_ANIMATION);
      } else if (lowerCmd.equalsIgnoreCase("clear")) {
        termClear();
        termFullRedraw();
      } else if (lowerCmd.startsWith("cowsay")) {
        String msg = "moo";
        if (cmdLine.length() > 6) {
          msg = cmdLine.substring(6);
          msg.trim();
        }
        if (msg.length() == 0) {
          msg = "moo";
        }
        cowsayActive = true;
        drawCowsayView(msg);
      } else {
        termAddChar(c);
      }
    } else {
      termAddChar(c);
    }
    return;
  }

  switch (c) {
    case '1': switchMode(MODE_ANIMATION); return;
    case '2': switchMode(MODE_CLOCK);     return;
    case '3': switchMode(MODE_POMODORO);  return;
    case '4': switchMode(MODE_TERMINAL);  return;
    case '5': switchMode(MODE_USAGE);     return;
    case '6': switchMode(MODE_WEATHER);   return;
    case 'b': setBacklight(!backlightOn); return;
    case 'U': collectingUsage = true; usageBuf = ""; return;
    case 'T': collectingClockSet = true; clockSetBuf = ""; return;
    case 'P': collectingPomoStart = true; pomoStartBuf = ""; return;
    case 'W': wxCollecting = true; wxBuf = ""; return;
    case 'I': collectingIdleInterval = true; idleBuf = ""; return;
    case '-':
      animSpeed = animSpeed > 1 ? animSpeed - 1 : 1;
      Serial.print("Speed: "); Serial.println(animSpeed);
      return;
    case '=':
      animSpeed = animSpeed < 3 ? animSpeed + 1 : 3;
      Serial.print("Speed: "); Serial.println(animSpeed);
      return;
  }

  switch (currentMode) {
    case MODE_ANIMATION: handleAnimationKey(c); break;
    case MODE_CLOCK:     handleClockKey(c);     break;
    case MODE_POMODORO:  handlePomodoroKey(c);  break;
    default: break;
  }
}

// ═════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  clockBaseMillis = millis();

  pinMode(TFT_BLK, OUTPUT);
  setBacklight(true);

  SPI.begin(8, -1, 10, TFT_CS);   // SCK=8, MOSI=10
  tft.init(240, 240);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);
  initColours();


  // ── Boot splash ────────────────────────────────────────────
  tft.fillScreen(animBgColor);
  tft.setTextColor(C_WHITE); tft.setTextSize(3);
  tft.setCursor(DISP_W / 2 - 54, DISP_H / 2 - 22); tft.print("Clawd");
  tft.setCursor(DISP_W / 2 - 54, DISP_H / 2 + 14); tft.print("Mochi");
  delay(1200);

  // ── Logo shown once at startup, then straight into Animation mode
  //    (dynamicMode defaults true, so it starts idle-cycling right away) ──
  animLogoReveal();
  switchMode(MODE_ANIMATION);
  idlePhaseAt  = millis();
  idlePhaseDur = (3 + random(3)) * 60000UL;

  Serial.println("Clawd Mochi ready.");
  Serial.println("Modes: 1=Animation 2=Clock 3=Pomodoro 4=Terminal 5=Usage 6=Weather");
  Serial.println("Global: b=toggle backlight  -/+ =speed down/up");
  Serial.println("Animation mode: w=normal s=squish z=logo m=dynamic");
  Serial.println("  e=blink f=double-blink g=look-around h=wink i=sleepy");
  Serial.println("  j=surprised k=squint l=nod n=shake o=roll u=cross-eyed");
  Serial.println("  v=tilt-confused x=excited");
  Serial.println("Clock mode: t=set time (HHMM)  r=set alarm (min from now)");
  Serial.println("Pomodoro mode: p=start/stop  P=MMSSB set+start (keeps ticking in background)");
  Serial.println("Terminal mode: type freely; \"exit\"+Enter to leave");
}

// ═════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════

// Idle cycle: ANIM (3-5 min) → CLOCK → WEATHER → USAGE → ANIM.
// Gated by dynamicMode — the same toggle as "Let Claude Vibe".
// Weather and Usage phases are skipped if no data has been pushed yet.
// Resets gracefully if the user manually changes mode.
void manageIdleCycle() {
  if (busy || alarmRinging || pomodoroRinging || timerRinging) return;

  uint32_t now = millis();

  bool onTrack = (idlePhase == 0 && currentMode == MODE_ANIMATION)
              || (idlePhase == 1 && currentMode == MODE_CLOCK)
              || (idlePhase == 2 && currentMode == MODE_WEATHER)
              || (idlePhase == 3 && currentMode == MODE_USAGE);

  if (!onTrack) {
    if (currentMode == MODE_ANIMATION) {
      idlePhase    = 0;
      idlePhaseAt  = now;
      idlePhaseDur = (3 + random(3)) * 60000UL;
    }
    return;
  }

  if (now - idlePhaseAt < idlePhaseDur) return;

  uint8_t next = (idlePhase + 1) % 4;
  if (next == 2 && !wx.hasData)       next = 3; // skip weather → try usage
  if (next == 3 && usageSessionPct < 0) next = 0; // skip usage → back to anim
  if (next == 2 && !wx.hasData)       next = 0; // both skipped

  idlePhase    = next;
  idlePhaseAt  = now;
  idlePhaseDur = (3 + random(3)) * 60000UL;

  switch (next) {
    case 0: switchMode(MODE_ANIMATION); break;
    case 1: switchMode(MODE_CLOCK);     break;
    case 2: switchMode(MODE_WEATHER);   break;
    case 3: switchMode(MODE_USAGE);     break;
  }
}

// Auto-popup the Usage screen (Animation mode only) the first time usage
// crosses each new 10% mark, plus a special one at 95% as a final warning
// before the limit. Uses whichever of session/weekly is higher, since
// either window can be the binding constraint.
int8_t lastUsageAlertBucket = -1;

void maybeAutoShowUsage() {
  if (usageSessionPct < 0) return;            // no data pushed yet
  if (currentMode != MODE_ANIMATION) return;  // don't interrupt other modes

  int8_t pct = usageSessionPct > usageWeeklyPct ? usageSessionPct : usageWeeklyPct;
  int8_t bucket = (pct >= 95) ? 95 : (pct / 10) * 10;
  if (bucket < 10 || bucket == lastUsageAlertBucket) return;

  lastUsageAlertBucket = bucket;
  drawUsageView();
  delay(4000);
  if (eyeView == EYE_SQUISH) drawSquishEyes(); else drawNormalEyes();
}

void loop() {
  while (Serial.available()) handleChar(Serial.read());

  checkAlarm();
  updateAlarmFlash();
  checkTimer();
  updateTimerFlash();
  updatePomodoro();
  updatePomoFlash();
  updatePomodoroIdleAnim();
  checkPomodoroTimeout();
  manageIdleCycle();
  maybeAutoShowUsage();
  updateClockViewIfShown();
  updateUsageViewIfShown();
  if (currentMode == MODE_WEATHER) updateWeatherView();

  if (dynamicMode && currentMode == MODE_ANIMATION && !busy && millis() >= nextIdleAnimAt) {
    eyeView = EYE_NORMAL;
    playRandomIdleAnim();
  }
}
