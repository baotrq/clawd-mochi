// Standalone test sketch — verifies each score API (football-data.org,
// balldontlie.io, RapidAPI Flashscore) works BEFORE wiring the result back
// into clawd_mochi/scores.ino. Not part of the shipped device firmware —
// flashing this TEMPORARILY REPLACES clawd_mochi on the board; reflash
// clawd_mochi/clawd_mochi.ino afterward to get the clock/alarms/etc back.
//
// Same TFT wiring as clawd_mochi (CS=2 DC=3 RST=4 BLK=1, SPI SCK=8 MOSI=10)
// so it can run on the same physical board without rewiring anything.
//
// Copy secrets.h.example -> secrets.h in this folder and fill in real
// values (same ones as clawd_mochi/secrets.h) before flashing.

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "secrets.h"

#define TFT_CS  2
#define TFT_DC  3
#define TFT_RST 4
#define TFT_BLK 1
#define DISP_W 240
#define DISP_H 240

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define RAPIDAPI_FLASHSCORE_HOST "flashscore4.p.rapidapi.com"

// ── On-screen scrolling log (very simple: print, advance Y, wipe+reset
// when it runs off the bottom — no real scrollback, this is a debug tool) ──
int16_t logY = 10;
#define LOG_LINE_H 10
#define LOG_MAX_Y (DISP_H - LOG_LINE_H)

void logLine(const String& msg) {
  Serial.println(msg);
  if (logY > LOG_MAX_Y) {
    tft.fillScreen(ST77XX_BLACK);
    logY = 10;
  }
  tft.setCursor(4, logY);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(1);
  String line = msg.length() > 40 ? msg.substring(0, 40) : msg;
  tft.print(line);
  logY += LOG_LINE_H;
}

// Formats (now - daysAgo days) as "YYYY-MM-DD" — same trick as scores.ino.
void formatDateDaysAgo(char* buf, size_t bufSize, int daysAgo) {
  time_t nowSecs;
  time(&nowSecs);
  time_t t = nowSecs - (time_t)daysAgo * 86400;
  struct tm tmv;
  localtime_r(&t, &tmv);
  snprintf(buf, bufSize, "%04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
}

// ── WiFi / NTP (blocking — fine for a one-off test tool) ────────────
void connectWiFiBlocking() {
  logLine("WiFi: connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(STA_SSID, STA_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    logLine("WiFi: connected, IP " + WiFi.localIP().toString());
  } else {
    logLine("WiFi: FAILED to connect (20s timeout)");
  }
}

void syncTimeBlocking() {
  if (WiFi.status() != WL_CONNECTED) return;
  logLine("NTP: syncing...");
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");
  struct tm ti;
  uint32_t start = millis();
  while (!getLocalTime(&ti, 500) && millis() - start < 15000) {
    Serial.print(".");
  }
  logLine((ti.tm_year > 100) ? "NTP: synced" : "NTP: FAILED (dates will be wrong)");
}

// ── football-data.org ────────────────────────────────────────────────
void testFootballDataOne(const char* code) {
  logLine(String("FD ") + code + ": requesting...");
  char dateFrom[11], dateTo[11];
  formatDateDaysAgo(dateFrom, sizeof(dateFrom), 3);
  formatDateDaysAgo(dateTo, sizeof(dateTo), 0);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String("https://api.football-data.org/v4/competitions/") + code + "/matches?dateFrom=" + dateFrom + "&dateTo=" + dateTo;
  if (!http.begin(client, url)) {
    logLine("FD begin failed");
    return;
  }
  http.addHeader("X-Auth-Token", FOOTBALL_DATA_API_KEY);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      int n = doc["matches"].as<JsonArray>().size();
      logLine(String("FD ") + code + ": HTTP 200, " + n + " matches");
    } else {
      logLine(String("FD ") + code + ": parse fail " + err.c_str());
    }
  } else {
    logLine(String("FD ") + code + ": HTTP " + httpCode);
  }
  http.end();
}

// Full 12-competition sweep, spaced 6.5s apart (football-data free tier
// caps at 10 req/min) — takes ~80s, fine for a deliberate manual test.
void testFootballDataFull() {
  static const char* codes[] = { "WC", "CL", "PL", "PD", "BL1", "SA", "FL1", "EC", "BSA", "PPL", "DED", "ELC" };
  logLine("FD: full sweep, ~80s...");
  for (int i = 0; i < 12; i++) {
    testFootballDataOne(codes[i]);
    if (i < 11) delay(6500);
  }
  logLine("FD: full sweep done.");
}

// ── balldontlie.io: NBA ──────────────────────────────────────────────
void testNba() {
  logLine("NBA: requesting...");
  String url = "https://api.balldontlie.io/v1/games?per_page=100";
  for (int d = 0; d <= 3; d++) {
    char dateStr[11];
    formatDateDaysAgo(dateStr, sizeof(dateStr), d);
    url += "&dates[]=" + String(dateStr);
  }
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) {
    logLine("NBA begin failed");
    return;
  }
  http.addHeader("Authorization", BALLDONTLIE_API_KEY);
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      int n = doc["data"].as<JsonArray>().size();
      logLine(String("NBA: HTTP 200, ") + n + " games");
    } else {
      logLine(String("NBA: parse fail ") + err.c_str());
    }
  } else {
    logLine(String("NBA: HTTP ") + httpCode);
  }
  http.end();
}

// ── RapidAPI Flashscore ──────────────────────────────────────────────
// Known-good sanity check: the exact endpoint + example match_id from your
// RapidAPI playground screenshot. Confirms key/host/auth actually work,
// independent of guessing the right search/list endpoint. NOTE: that
// match_id was live/recent when you captured the screenshot — by the time
// you run this it may have aged out of their cache. A 404/empty result
// here just means the match_id is stale; a 401/403 means auth is actually
// broken (wrong key/host).
void testFlashscoreMomentum() {
  logLine("Flashscore Momentum: requesting known match_id...");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String("https://") + RAPIDAPI_FLASHSCORE_HOST + "/api/flashscore/v2/matches/momentum?match_id=bc27lzfo";
  if (!http.begin(client, url)) {
    logLine("Momentum begin failed");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-rapidapi-host", RAPIDAPI_FLASHSCORE_HOST);
  http.addHeader("x-rapidapi-key", RAPIDAPI_FLASHSCORE_KEY);
  int httpCode = http.GET();
  logLine(String("Momentum: HTTP ") + httpCode);
  String payload = http.getString();
  Serial.println("---- Momentum response body ----");
  Serial.println(payload);
  Serial.println("---- end ----");
  logLine(payload.substring(0, payload.length() > 100 ? 100 : payload.length()));
  http.end();
}

// General-purpose explorer: type any path (e.g.
// "/api/flashscore/v2/search?q=Europa") and see the raw response — the
// main tool for iterating on the unknown search/tournament-live paths
// without recompiling/reflashing per guess. Full body goes to Serial only
// (usually too long for the screen).
void testRawPath(const String& path) {
  logLine("RAW: " + path);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String("https://") + RAPIDAPI_FLASHSCORE_HOST + path;
  if (!http.begin(client, url)) {
    logLine("RAW begin failed");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-rapidapi-host", RAPIDAPI_FLASHSCORE_HOST);
  http.addHeader("x-rapidapi-key", RAPIDAPI_FLASHSCORE_KEY);
  int httpCode = http.GET();
  logLine(String("RAW: HTTP ") + httpCode);
  String payload = http.getString();
  Serial.println("---- RAW response body ----");
  Serial.println(payload);
  Serial.println("---- end ----");
  logLine(payload.substring(0, payload.length() > 100 ? 100 : payload.length()));
  http.end();
}

// ── Menu / dispatch ───────────────────────────────────────────────────
void printMenu() {
  logLine("== Score API Test Tool ==");
  logLine("1=FD World Cup (quick)");
  logLine("2=FD full sweep, 12 comps ~80s");
  logLine("3=NBA (quick)");
  logLine("4=Flashscore Momentum (known id)");
  logLine("5=Flashscore raw path (type+Enter)");
  logLine("M=show this menu again");
}

bool collectingPath = false;
String pathBuf = "";

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH);
  SPI.begin(8, -1, 10, TFT_CS);  // SCK=8, MOSI=10 — same as clawd_mochi
  tft.init(240, 240);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  connectWiFiBlocking();
  syncTimeBlocking();
  printMenu();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (collectingPath) {
      if (c == '\n' || c == '\r') {
        if (pathBuf.length() > 0) testRawPath(pathBuf);
        collectingPath = false;
        pathBuf = "";
      } else if (c >= 32 && c <= 126 && pathBuf.length() < 200) {
        pathBuf += c;
      }
      continue;
    }
    switch (c) {
      case '1': testFootballDataOne("WC"); break;
      case '2': testFootballDataFull(); break;
      case '3': testNba(); break;
      case '4': testFlashscoreMomentum(); break;
      case '5':
        collectingPath = true;
        pathBuf = "";
        logLine("Type RapidAPI path, then Enter:");
        break;
      case 'M':
      case 'm':
        printMenu();
        break;
      default: break;
    }
  }
}
