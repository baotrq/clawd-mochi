// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  WIFI + NTP (background, best-effort real time)
// ═════════════════════════════════════════════════════════════
// Fills in real wall-clock time over WiFi + NTP so the clock isn't stuck at
// 00:00 or a manually-typed guess. Purely a backup source — this board's
// radio is known to be flaky (see CLAUDE.md), so nothing here ever blocks
// anything: it's a small state machine stepped once per loop() off
// millis(), and if WiFi never connects or NTP never answers, the existing
// millis()-based clock and Serial 't'/'T' set commands work exactly as
// they always have.
//
// Note: the TZ offset itself is set once, unconditionally, in setup()
// (clawd_mochi.ino, via setenv("TZ", ...)) — not here, and not gated on WiFi
// connecting. It must apply from boot regardless of network state, since
// Serial-side time syncs (the web app's silent 'T'+epoch push) can and do
// land before WiFi/NTP ever gets a turn. configTime() below re-applies the
// same offset (harmless) and, more importantly, is what actually registers
// the NTP servers and kicks off a sync — that part *does* need to wait for
// a real WiFi connection.

#define WIFI_CONNECT_TIMEOUT_MS 15000UL          // give up on one attempt after this long
#define WIFI_RETRY_INTERVAL_MS  (5UL * 60000UL)  // wait this long before trying again

enum WifiTimeState { WT_IDLE, WT_CONNECTING, WT_WAITING_NTP };
WifiTimeState wifiTimeState   = WT_IDLE;
// Underflows on purpose: makes the very first `millis() - wifiTimeStateAt`
// check in WT_IDLE true immediately, so boot doesn't wait a full
// WIFI_RETRY_INTERVAL_MS (5 min) before the first connect attempt.
uint32_t      wifiTimeStateAt = 0 - WIFI_RETRY_INTERVAL_MS;

void startWifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);               // mitigates this board's known-flaky radio
  WiFi.setTxPower(WIFI_POWER_8_5dBm); // same mitigation, lower TX power
  WiFi.begin(STA_SSID, STA_PASS);
  wifiTimeState   = WT_CONNECTING;
  wifiTimeStateAt = millis();
  Serial.print("WiFi: connecting to ");
  Serial.print(STA_SSID);
  Serial.println(" ...");
}

// Stepped once per loop(). Cheap no-op once real time is synced, or if no
// credentials were ever configured (STA_SSID left blank).
void updateWifiTime() {
  if (timeSynced) return;           // already have real time (NTP or manual Serial set)
  if (STA_SSID[0] == '\0') return;  // no WiFi configured — stay fully offline

  switch (wifiTimeState) {
    case WT_IDLE:
      if (millis() - wifiTimeStateAt >= WIFI_RETRY_INTERVAL_MS) startWifiConnect();
      break;

    case WT_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi: connected, IP ");
        Serial.println(WiFi.localIP());
        Serial.println("WiFi: requesting NTP time...");
        // TZ was already set in setup() (see note above); this both
        // re-applies it (harmless) and, now that we actually have a network
        // connection, registers the NTP servers and starts SNTP.
        configTime(TZ_OFFSET_SEC, 0, "pool.ntp.org", "time.google.com");
        wifiTimeState   = WT_WAITING_NTP;
        wifiTimeStateAt = millis();
      } else if (millis() - wifiTimeStateAt >= WIFI_CONNECT_TIMEOUT_MS) {
        WiFi.disconnect(true);
        wifiTimeState   = WT_IDLE;
        wifiTimeStateAt = millis();  // next attempt after WIFI_RETRY_INTERVAL_MS
        Serial.println("WiFi: connect timed out, will retry in 5 min.");
      }
      break;

    case WT_WAITING_NTP: {
      struct tm ti;
      if (getLocalTime(&ti, 0)) {    // 0ms timeout = poll only, never blocks
        timeSynced = true;
        Serial.println("WiFi: time synced via NTP. Got internet.");
        if (currentMode == MODE_CLOCK) drawClockView();
      } else if (millis() - wifiTimeStateAt >= WIFI_CONNECT_TIMEOUT_MS) {
        WiFi.disconnect(true);       // NTP never answered — back off and retry later
        wifiTimeState   = WT_IDLE;
        wifiTimeStateAt = millis();
        Serial.println("WiFi: connected but NTP never answered, will retry in 5 min.");
      }
      break;
    }
  }
}
