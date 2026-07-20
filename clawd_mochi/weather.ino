// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  LIVE WEATHER (MODE_WEATHER)
//  OpenWeatherMap current-weather fetched directly by the device every
//  10 min, for the lat/lon last pushed by the web app's 'L' command (see
//  mode_switching.ino) — a control action, not a data push. Same pattern
//  as scores.ino uses for live sports data.
//  wx / wxLoc / wxCondStr / wxLat / wxLon live in clawd_mochi.ino.
// ═════════════════════════════════════════════════════════════

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Mirrors the web app's (now-removed) owIdToWmo() mapping so the existing
// wmoToCondition() in clawd_mochi.ino keeps working unchanged.
static uint16_t owIdToWmo(int id) {
  if (id == 800) return 0;                 // clear sky
  if (id >= 801 && id <= 804) return 2;     // clouds
  if (id >= 700 && id < 800) {
    if (id == 771 || id == 781) return 771; // windy
    return 45;                              // mist/fog/haze
  }
  if (id >= 600 && id < 700) return 71;     // snow
  if (id >= 300 && id < 600) return 61;     // drizzle, rain
  if (id >= 200 && id < 300) return 95;     // thunderstorm
  return 2;
}

// Title-cases an OpenWeatherMap description ("light rain" -> "Light Rain").
static void titleCase(String& s) {
  bool atWordStart = true;
  for (size_t i = 0; i < s.length(); i++) {
    if (s[i] == ' ') { atWordStart = true; continue; }
    if (atWordStart) s[i] = toupper(s[i]);
    atWordStart = false;
  }
}

#define WEATHER_FETCH_INTERVAL_MS (10UL * 60000)
// Underflow trick (same idea as wifi_time.ino) so the first fetch fires
// promptly after boot instead of waiting a full interval.
static uint32_t lastWeatherFetchMs = 0 - WEATHER_FETCH_INTERVAL_MS;

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Weather: WiFi not connected, skipping.");
    return;
  }
  if (OPENWEATHER_API_KEY[0] == '\0') {
    Serial.println("Weather: no OPENWEATHER_API_KEY, skipping.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();  // matches scores.ino/wifi_time.ino — skip cert verification for simplicity
  HTTPClient http;
  // String(float) truncates to 2 decimals by default (~1km error) — pass
  // explicit precision so the fetched location matches what was picked.
  String url = String("https://api.openweathermap.org/data/2.5/weather?lat=") + String(wxLat, 4) +
               "&lon=" + String(wxLon, 4) + "&units=metric&appid=" + OPENWEATHER_API_KEY;
  if (!http.begin(client, url)) {
    Serial.println("Weather: begin failed");
    return;
  }
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument filter;
    filter["main"]["temp"] = true;
    filter["main"]["feels_like"] = true;
    filter["main"]["humidity"] = true;
    filter["weather"][0]["id"] = true;
    filter["weather"][0]["description"] = true;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
    if (!err) {
      wx.tempC = (int8_t)round((double)(doc["main"]["temp"] | 0.0));
      wx.feelsC = (int8_t)round((double)(doc["main"]["feels_like"] | 0.0));
      wx.humidity = doc["main"]["humidity"] | 0;
      int owId = doc["weather"][0]["id"] | 800;
      wx.cond = wmoToCondition(owIdToWmo(owId));
      wx.hasData = true;

      wxCondStr = (const char*)(doc["weather"][0]["description"] | "cloudy");
      titleCase(wxCondStr);
      if (wxCondStr.length() > 20) wxCondStr = wxCondStr.substring(0, 20);

      if (currentMode == MODE_WEATHER) drawWeatherView();
    } else {
      Serial.printf("Weather: JSON parse failed: %s\n", err.c_str());
    }
  } else {
    Serial.printf("Weather: HTTP %d\n", httpCode);
  }
  http.end();
}

void updateWeather() {
  uint32_t now = millis();
  if (now - lastWeatherFetchMs < WEATHER_FETCH_INTERVAL_MS) return;
  lastWeatherFetchMs = now;
  fetchWeather();
}
