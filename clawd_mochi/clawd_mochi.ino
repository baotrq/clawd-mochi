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
 *     6 = Weather mode       7 = Sing mode (buzzer jukebox)
 *
 *   ── SING MODE (buzzer on GPIO5) ───────────────────────────────
 *     q w e r t y = pick & play a song  space/o = play/pause
 *     x = stop                          m = toggle loop
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
 *     t = set time (HHMM)
 *     r = set alarm  (HHMM + Enter; screen flashes when it rings)
 *     y = set timer  (seconds + Enter; counts down then flashes)
 *         Alarm & timer take an OPTIONAL name after a space, shown on the
 *         ring screen instead of ALARM!/TIMER!  e.g. "0730 Wake up",
 *         "300 Tea".  No name → default label.
 *     R = ring the alarm now (web fires this at T-0; 'R <name>\n' to label
 *         the ring, plain 'R\n' uses the default)
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
 *   (no RTC — clock runs off millis(), defaults to 00:00 at boot, unless
 *    STA_SSID/STA_PASS are set, in which case WiFi+NTP fills in real time
 *    in the background as soon as it connects — see wifi_time.ino)
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <WiFi.h>

// ── WiFi (optional, background NTP time backup — see wifi_time.ino) ────
// Credentials live in secrets.h (gitignored, not committed) so a real WiFi
// password never lands in git history. Copy secrets.h.example to secrets.h
// in this same folder and fill in STA_SSID/STA_PASS; leave both blank to
// skip WiFi entirely and rely on millis()/Serial 't'/'T' as before.
#include "secrets.h"
#define TZ_OFFSET_SEC (7 * 3600)   // Ho Chi Minh City, ICT, UTC+7, no DST

// ── BLE & Dual Serial Wrapper ─────────────────────────────────
void initBLE();
void bleSend(const String& msg);

class DualSerialClass {
public:
  void begin(unsigned long baud) {
    Serial.begin(baud);
  }
  int available() {
    return Serial.available();
  }
  int read() {
    return Serial.read();
  }
  int peek() {
    return Serial.peek();
  }
  void flush() {
    Serial.flush();
  }
  
  template<typename T>
  size_t print(T val) {
    size_t r = Serial.print(val);
    bleSend(String(val));
    return r;
  }
  
  size_t print(const char* val) {
    size_t r = Serial.print(val);
    bleSend(String(val));
    return r;
  }

  size_t print(const String& val) {
    size_t r = Serial.print(val);
    bleSend(val);
    return r;
  }

  template<typename T>
  size_t println(T val) {
    size_t r = Serial.println(val);
    bleSend(String(val) + "\n");
    return r;
  }
  
  size_t println(const char* val) {
    size_t r = Serial.println(val);
    bleSend(String(val) + "\n");
    return r;
  }

  size_t println(const String& val) {
    size_t r = Serial.println(val);
    bleSend(val + "\n");
    return r;
  }

  size_t println() {
    size_t r = Serial.println();
    bleSend("\n");
    return r;
  }
};

DualSerialClass DualSerial;
#define Serial DualSerial

// ── Forward Declarations ──────────────────────────────────────
void handleChar(char c);
void drawNormalEyes(int16_t ox = 0, bool blink = false, int16_t oy = 0);
void drawEyesAsym(int16_t lxOff, int16_t lyOff, int16_t rxOff, int16_t ryOff, bool blink = false);
void drawSquishEyes(bool closed = false);
void drawWinkEyes(bool leftWink);
void drawDroopyEyes(int16_t openH);

// ── Pins ──────────────────────────────────────────────────────
#define TFT_CS  2
#define TFT_DC  3
#define TFT_RST 4
#define TFT_BLK 1
#define BUZZER_PIN 5   // passive piezo buzzer (+ → GPIO5, - → GND); driven by tone()

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
enum Mode { MODE_ANIMATION, MODE_CLOCK, MODE_POMODORO, MODE_TERMINAL, MODE_USAGE, MODE_WEATHER, MODE_SING };
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

// ── Clock (using standard C library time.h — synced from Web Serial)
bool     timeSynced         = false;
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
uint32_t pomoRingStartAt    = 0;
uint32_t pomoWorkMs         = 25UL * 60000;
uint32_t pomoBreakMs        =  5UL * 60000;
uint32_t pomoStoppedAt      = 0;
uint8_t  pomoIdlePhase      = 0;   // idle tomato animation phase
uint8_t  pomoIdleStep       = 0;
uint32_t pomoIdleNextAt     = 0;

// ── Alarm ────────────────────────────────────────────────────────────
bool     alarmArmed    = false;
bool     alarmRinging    = false;
uint32_t alarmAtMillis   = 0;
uint32_t alarmFlashAt    = 0;
bool     alarmFlashOn    = false;
uint32_t alarmRingStartAt = 0;

// ── Timer ────────────────────────────────────────────────────────────
bool     timerActive     = false;
bool     timerRinging    = false;
uint32_t timerAtMillis   = 0;
uint32_t timerDurationMs = 0;
uint32_t timerFlashAt    = 0;
bool     timerFlashOn    = false;
uint32_t timerRingStartAt = 0;

// Optional labels shown on the flashing ring screen ("" → default ALARM!/TIMER!).
// Set over Serial by typing a name after the time/duration (e.g. "0730 Wake up",
// "300 Tea"), or pushed by the web with the ring trigger ('R <name>').
String   alarmName = "";
String   timerName = "";
// The web owns alarm timing (no RTC on this board) and fires the ring via 'R',
// optionally carrying a name — collected here char-by-char until newline.
bool     collectingAlarmRing = false;
String   alarmRingBuf        = "";

// ── Cowsay ───────────────────────────────────────────────────────────
bool     cowsayActive    = false;

// ── Sing mode ────────────────────────────────────────────────────────
// Non-blocking jukebox: a song is a list of (frequency, note-divisor) pairs
// (see sing.ino). updateSing() advances one note at a time off millis() so
// loop() keeps pumping serial/web. State lives here (not in sing.ino) because
// mode_switching.ino is concatenated before sing.ino and needs to see it.
int8_t   singSong    = 0;      // which song is selected (index into SONGS[])
bool     singPlaying = false;  // is a note sequence currently sounding?
uint16_t singNoteIdx = 0;      // index of the note currently playing
uint32_t singNoteAt  = 0;      // millis() when the current note started
uint16_t singNoteMs  = 0;      // how long the current note lasts
bool     singLoop    = true;   // restart the song when it finishes
uint32_t singBarAt   = 0;      // last equalizer-bar repaint time
uint8_t  singBars[12] = {0};   // current equalizer bar heights


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
enum WeatherCondition { WC_CLEAR, WC_CLOUDY, WC_FOG, WC_RAIN, WC_STORM, WC_SNOWY, WC_WINDY };
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
int16_t  windPersonX = 50;

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
//  SETUP
// ═════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  initBLE();
  randomSeed(esp_random());

  // Set the Vietnam (ICT, UTC+7) offset unconditionally, up front, via the
  // plain libc mechanism (no network/SNTP touched — safe before WiFi is
  // ever initialized). This makes every localtime_r()/mktime() call correct
  // from boot onward regardless of whether/when WiFi+NTP (wifi_time.ino)
  // connects. It must not be gated behind a successful WiFi connection: the
  // web app's own silent time syncs ('T' + epoch, sent over Serial right on
  // connect) race ahead of WiFi/NTP and would otherwise "win" first, setting
  // timeSynced=true and permanently skipping wifi_time.ino's configTime()
  // call for the rest of the boot session — leaving the clock stuck on
  // raw UTC (7h behind) even though the underlying time itself is correct.
  setenv("TZ", "ICT-7", 1);
  tzset();


  pinMode(TFT_BLK, OUTPUT);
  setBacklight(true);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);   // passive buzzer idles silent

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
  startupChime();   // rising power-on jingle while the splash shows
  delay(660);       // chime ~540ms + this ≈ original 1200ms splash hold

  // ── Logo shown once at startup, then straight into Animation mode
  //    (dynamicMode defaults true, so it starts idle-cycling right away) ──
  animLogoReveal();
  switchMode(MODE_ANIMATION);
  idlePhaseAt  = millis();
  idlePhaseDur = random(30, 90) * 1000UL;  // first glance in 30-90 s

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
  Serial.println("Sing mode (7): q/w/e/r/t/y pick song  space=play/pause  x=stop  m=loop");
}

// ═════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════

// Idle cycle: ANIM (3-5 min) → CLOCK → WEATHER → USAGE → ANIM.
// Clawd idles in animation mode, then randomly glances at clock / weather /
// usage for a short random time, then returns to animation.
// idlePhase 0 = lounging in animation, 1 = mid-glance.
void manageIdleCycle() {
  if (busy || alarmRinging || pomodoroRinging || timerRinging) return;

  uint32_t now = millis();

  if (idlePhase == 0) {
    if (currentMode != MODE_ANIMATION) return; // user navigated away — hands off
    if (now - idlePhaseAt < idlePhaseDur) return;

    // Pick a random info screen to glance at
    uint8_t opts[3]; uint8_t n = 0;
    opts[n++] = 1;                            // clock always available
    if (wx.hasData)          opts[n++] = 2;  // weather if we have data
    if (usageSessionPct >= 0) opts[n++] = 3; // usage if pushed

    uint8_t pick = opts[random(n)];
    idlePhase    = 1;
    idlePhaseAt  = now;
    idlePhaseDur = random(15, 40) * 1000UL;  // glance for 15-40 s

    switch (pick) {
      case 1: switchMode(MODE_CLOCK);   break;
      case 2: switchMode(MODE_WEATHER); break;
      case 3: switchMode(MODE_USAGE);   break;
    }
  } else {
    // Mid-glance — if user went back early, respect that
    if (currentMode == MODE_ANIMATION) {
      idlePhase    = 0;
      idlePhaseAt  = now;
      idlePhaseDur = random(60, 180) * 1000UL;
      return;
    }
    if (now - idlePhaseAt < idlePhaseDur) return;

    // Done glancing — return to animation and wait before next glance
    idlePhase    = 0;
    idlePhaseAt  = now;
    idlePhaseDur = random(60, 180) * 1000UL;  // lounge 1-3 min before next
    switchMode(MODE_ANIMATION);
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

  updateWifiTime();

  checkAlarm();
  updateAlarmFlash();
  checkTimer();
  updateTimerFlash();
  if (!alarmRinging && !timerRinging) {
    updatePomodoro();
    updatePomoFlash();
    updatePomodoroIdleAnim();
    checkPomodoroTimeout();
    manageIdleCycle();
    maybeAutoShowUsage();
    updateClockViewIfShown();
    updateUsageViewIfShown();
    if (currentMode == MODE_WEATHER) updateWeatherView();
    updateSing();
    updateSingView();

    if (dynamicMode && currentMode == MODE_ANIMATION && !busy && millis() >= nextIdleAnimAt) {
      eyeView = EYE_NORMAL;
      playRandomIdleAnim();
    }
  }
}
