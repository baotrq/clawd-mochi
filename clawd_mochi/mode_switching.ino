// Part of the Clawd Mochi sketch (see clawd_mochi.ino for pins, globals, setup() and loop()).
// Arduino's IDE concatenates every .ino file in this folder into one build, so this is
// still a single flashed program — split into files purely for easier reading/debugging.

// ═════════════════════════════════════════════════════════════
//  MODE SWITCHING
// ═════════════════════════════════════════════════════════════

// Single source of truth for "draw whatever the current mode looks like" —
// used both for actual mode switches and for redraws after an overlay
// (alarm / numeric input) closes.
void switchMode(Mode m) {
  // Leaving Sing mode silences the buzzer so a melody never bleeds over the
  // clock/alarm. (Redraws within Sing pass MODE_SING, so playback survives those.)
  if (m != MODE_SING) stopSong();
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
    case MODE_SING:
      drawSingView();
      Serial.println("MODE:SING");
      break;
    case MODE_SCORES:
      drawScoresView();
      Serial.println("MODE:SCORES");
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
    case 'y': eyeView = EYE_NORMAL; animHypnoEyes(); break;
    case 'c': eyeView = EYE_NORMAL; animCryEyes(); break;
    case 'd': eyeView = EYE_NORMAL; animNekoEyes(); break;
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
  extern bool collectingAlarms;
  extern String alarmsBuf;
  
  if (collectingAlarms) {
    if (c == '\n' || c == '\r') {
      alarmsBuf.trim();
      if (alarmsBuf.length() > 0) {
        if (alarmsBuf.startsWith("C")) {
          for (int i = 0; i < 10; i++) {
            alarmsList[i].enabled = false;
            alarmsList[i].hour = 0;
            alarmsList[i].minute = 0;
            alarmsList[i].days = 0;
            memset(alarmsList[i].name, 0, sizeof(alarmsList[i].name));
          }
          Serial.println("Alarms cleared (staged, send E to commit).");
        } else if (alarmsBuf.startsWith("E")) {
          // Commit whatever's currently staged in alarmsList to flash in one
          // write. A full "A C" + "A S ..." x N + "A E" resync only persists
          // if the whole sequence lands — a drop mid-sequence (flaky
          // BLE/WiFi) leaves flash on its last good state instead of
          // silently wiping stored alarms.
          saveAlarmsToFlash();
          Serial.println("Alarms committed to flash.");
        } else if (alarmsBuf.startsWith("S ")) {
          String params = alarmsBuf.substring(2);
          int firstSpace = params.indexOf(' ');
          int secondSpace = params.indexOf(' ', firstSpace + 1);
          int thirdSpace = params.indexOf(' ', secondSpace + 1);
          int fourthSpace = params.indexOf(' ', thirdSpace + 1);
          int fifthSpace = params.indexOf(' ', fourthSpace + 1);
          
          if (firstSpace != -1 && secondSpace != -1 && thirdSpace != -1 && fourthSpace != -1) {
            int idx = params.substring(0, firstSpace).toInt();
            bool enabled = params.substring(firstSpace + 1, secondSpace).toInt() == 1;
            uint8_t hour = params.substring(secondSpace + 1, thirdSpace).toInt();
            uint8_t minute = params.substring(thirdSpace + 1, fourthSpace).toInt();
            uint8_t days = 0;
            String name = "";
            
            if (fifthSpace != -1) {
              days = params.substring(fourthSpace + 1, fifthSpace).toInt();
              name = params.substring(fifthSpace + 1);
            } else {
              days = params.substring(fourthSpace + 1).toInt();
            }
            
            if (idx >= 0 && idx < 10) {
              alarmsList[idx].enabled = enabled;
              alarmsList[idx].hour = hour;
              alarmsList[idx].minute = minute;
              alarmsList[idx].days = days;
              name.trim();
              if (name.length() > 23) name = name.substring(0, 23);
              memset(alarmsList[idx].name, 0, sizeof(alarmsList[idx].name));
              memcpy(alarmsList[idx].name, name.c_str(), name.length());

              Serial.printf("Alarm [%d] staged: %02d:%02d, Days: %d, Name: %s\n", idx, hour, minute, days, alarmsList[idx].name);
            }
          }
        }
      }
      collectingAlarms = false;
      alarmsBuf = "";
    } else if (c >= 32 && c <= 126 && alarmsBuf.length() < 64) {
      alarmsBuf += c;
    }
    return;
  }

  if (cowsayActive) {
    cowsayActive = false;
    switchMode(currentMode);
    return;
  }

  // Handle in-progress data streams before alarm/timer dismiss so that
  // automatic web pushes (U=usage, W=weather) cannot kill the alarm ring.
  if (wxCollecting) {
    if (c == '\n' || c == '\r') {
      int comma1 = wxBuf.indexOf(',');
      int comma2 = wxBuf.indexOf(',', comma1 + 1);
      int comma3 = wxBuf.indexOf(',', comma2 + 1);
      if (comma1 != -1 && comma2 != -1 && comma3 != -1) {
        uint16_t wmo = wxBuf.substring(0, comma1).toInt();
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
          if (wxLoc.length() > 20) wxLoc = wxLoc.substring(0, 20);
          wxCondStr.trim();
          if (wxCondStr.length() > 20) wxCondStr = wxCondStr.substring(0, 20);
          if (wxLoc.length() == 0 || wxLoc == "HCMC") wxLoc = "Ho Chi Minh City";
        } else {
          hum = wxBuf.substring(comma3 + 1).toInt();
          wxCondStr = "";
          if (wxLoc.length() == 0 || wxLoc == "HCMC") wxLoc = "Ho Chi Minh City";
        }
        wx.cond = wmoToCondition(wmo);
        wx.tempC = temp;
        wx.feelsC = feels;
        wx.humidity = hum;
        wx.hasData = true;
        if (currentMode == MODE_WEATHER) drawWeatherView();
      }
      wxCollecting = false;
      wxBuf = "";
    } else if (c >= 32 && c <= 126 && wxBuf.length() < 64) {
      wxBuf += c;
    }
    return;
  }

  // Weather location push ('L' command) — "<lat>,<lon>,<name>\n". A control
  // action (the user picked a location), not weather data itself — the
  // device fetches the actual conditions for this lat/lon on its own timer
  // (weather.ino), same pattern as 'F' favorite teams does for scores.ino.
  if (locCollecting) {
    if (c == '\n' || c == '\r') {
      int comma1 = locBuf.indexOf(',');
      if (comma1 != -1) {
        int comma2 = locBuf.indexOf(',', comma1 + 1);
        wxLat = locBuf.substring(0, comma1).toFloat();
        String name;
        if (comma2 != -1) {
          wxLon = locBuf.substring(comma1 + 1, comma2).toFloat();
          name = locBuf.substring(comma2 + 1);
        } else {
          wxLon = locBuf.substring(comma1 + 1).toFloat();
        }
        name.trim();
        if (name.length() > 20) name = name.substring(0, 20);
        wxLoc = name.length() ? name : "Location";
        saveWeatherLocationToFlash();
        Serial.printf("Weather location set: %.4f,%.4f (%s)\n", wxLat, wxLon, wxLoc.c_str());
        fetchWeather();  // refresh immediately for the new location
      }
      locCollecting = false;
      locBuf = "";
    } else if (c >= 32 && c <= 126 && locBuf.length() < 48) {
      locBuf += c;
    }
    return;
  }

  // Favorite teams push ('F' command) — pipe-separated (not comma; team
  // names may legitimately contain commas), used to pin a live favorite
  // game when rotating the Scores view (see scores.ino).
  if (favTeamsCollecting) {
    if (c == '\n' || c == '\r') {
      favoriteTeamCount = 0;
      int start = 0;
      while (start <= (int)favTeamsBuf.length() && favoriteTeamCount < MAX_FAVORITE_TEAMS) {
        int bar = favTeamsBuf.indexOf('|', start);
        String team = (bar == -1) ? favTeamsBuf.substring(start) : favTeamsBuf.substring(start, bar);
        team.trim();
        if (team.length() > 0) {
          if (team.length() > 23) team = team.substring(0, 23);
          memset(favoriteTeams[favoriteTeamCount], 0, sizeof(favoriteTeams[favoriteTeamCount]));
          memcpy(favoriteTeams[favoriteTeamCount], team.c_str(), team.length());
          favoriteTeamCount++;
        }
        if (bar == -1) break;
        start = bar + 1;
      }
      Serial.printf("Favorite teams set (%d).\n", favoriteTeamCount);
      saveFavoriteTeamsToFlash();
      favTeamsCollecting = false;
      favTeamsBuf = "";
    } else if (c >= 32 && c <= 126 && favTeamsBuf.length() < 160) {
      favTeamsBuf += c;
    }
    return;
  }

  if (collectingUsage) {
    if (c == '\n' || c == '\r') {
      if (usageBuf.length() == 12) {
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

  // Ring the alarm. The web has accurate time (the ESP has no RTC and drifts),
  // so it owns alarm timing and sends 'R' at its own countdown T-0 — no
  // drifting ESP-side countdown, no input-prompt flash. An optional name may
  // follow ('R Wake up\n'); it is collected until newline, then shown on the
  // ring screen. Plain 'R\n' rings with the default label.
  if (collectingAlarmRing) {
    if (c == '\n' || c == '\r') {
      alarmRingBuf.trim();
      if (alarmRingBuf.length() > 18) alarmRingBuf = alarmRingBuf.substring(0, 18);
      alarmName        = alarmRingBuf;   // "" → default ALARM!
      alarmArmed       = false;
      alarmRinging     = true;
      alarmFlashAt     = 0;
      alarmRingStartAt = millis();
      collectingAlarmRing = false;
      alarmRingBuf = "";
    } else if (c >= 32 && c <= 126 && alarmRingBuf.length() < 20) {
      alarmRingBuf += c;
    }
    return;
  }
  if (c == 'R') {
    collectingAlarmRing = true;
    alarmRingBuf = "";
    return;
  }

  // Dismiss alarm/timer on any real user key. Ignore protocol/whitespace
  // bytes so they don't kill the ring: 'U'/'W' are auto-pushes, 'R' is the
  // ring trigger itself, and '\n'/'\r' are command terminators (the 'R\n'
  // that starts the ring must not immediately dismiss it).
  if (alarmRinging || timerRinging) {
    if (c == 'U' || c == 'W' || c == 'R' || c == '\n' || c == '\r') return;
    if (alarmRinging) { dismissAlarm(); return; }
    if (timerRinging) { dismissTimer(); return; }
  }
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

  // Debug/validation only: 'D' + index + Enter plays that IDLE_ANIMS[] entry
  // immediately (any mode, any time), echoing its name — so a new animation
  // can be checked over the Serial Monitor without waiting for dynamicMode's
  // random idle-cycle to land on it.
  if (collectingAnimTest) {
    if (c == '\n' || c == '\r') {
      int idx = animTestBuf.toInt();
      if (idx >= 0 && idx < IDLE_ANIM_COUNT) {
        Serial.printf("Playing anim [%d]: %s\n", idx, IDLE_ANIM_NAMES[idx]);
        IDLE_ANIMS[idx]();
      } else {
        Serial.printf("Index out of range. Valid: 0-%d\n", IDLE_ANIM_COUNT - 1);
      }
      collectingAnimTest = false;
      animTestBuf = "";
    } else if (c >= '0' && c <= '9') {
      animTestBuf += c;
    }
    return;
  }

  if (collectingClockSet) {
    if (c == '\n' || c == '\r') {
      if (clockSetBuf.length() >= 10) {
        time_t epoch = (time_t)strtoul(clockSetBuf.c_str(), NULL, 10);
        struct timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        timeSynced = true;
        if (currentMode == MODE_CLOCK) drawClockView(); // keep it accurate if already shown
      } else if (clockSetBuf.length() == 6 || clockSetBuf.length() == 4) {
        uint8_t hh = clockSetBuf.substring(0, 2).toInt() % 24;
        uint8_t mm = clockSetBuf.substring(2, 4).toInt(); if (mm > 59) mm = 59;
        uint8_t ss = clockSetBuf.length() == 6 ? clockSetBuf.substring(4, 6).toInt() : 0;
        if (ss > 59) ss = 59;

        // This payload carries only a time-of-day, no date — keep whatever
        // date the device already has (from an earlier full-epoch sync, NTP,
        // or the 1970 default) and only overwrite hour/min/sec. A hardcoded
        // date here previously caused the display to jump to a fixed day.
        time_t nowSecs;
        time(&nowSecs);
        struct tm t;
        localtime_r(&nowSecs, &t);
        t.tm_hour = hh;
        t.tm_min = mm;
        t.tm_sec = ss;
        t.tm_isdst = -1;
        time_t epoch = mktime(&t);
        
        struct timeval tv;
        tv.tv_sec = epoch;
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
        timeSynced = true;
        if (currentMode == MODE_CLOCK) drawClockView(); // keep it accurate if already shown
      }
      collectingClockSet = false;
      clockSetBuf = "";
    } else if (c >= '0' && c <= '9' && clockSetBuf.length() < 12) {
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
    case '7': switchMode(MODE_SING);      return;
    case '8': switchMode(MODE_SCORES);    return;
    case 'b': setBacklight(!backlightOn); return;
    case 'U': collectingUsage = true; usageBuf = ""; return;
    case 'T': collectingClockSet = true; clockSetBuf = ""; return;
    case 'P': collectingPomoStart = true; pomoStartBuf = ""; return;
    case 'W': wxCollecting = true; wxBuf = ""; return;
    case 'L': locCollecting = true; locBuf = ""; return;
    case 'F': favTeamsCollecting = true; favTeamsBuf = ""; return;
    case 'I': collectingIdleInterval = true; idleBuf = ""; return;
    case 'A': collectingAlarms = true; alarmsBuf = ""; return;
    case 'D': collectingAnimTest = true; animTestBuf = ""; return;
    case 'Z':
      // Debug/validation only: force all three score sources to fetch right
      // now, bypassing the 5min/2h timers, so each API can be checked over
      // Serial Monitor without waiting on a real live/recent match. Blocks
      // for ~80s (12 football-data calls, spaced to respect their 10/min
      // limit) — fine here since it's a manual, one-off debug trigger, not
      // something running unattended. Also burns a RapidAPI call against
      // the 500/month cap each time — use sparingly, not something to spam.
      Serial.println("Scores: manual test fetch (Z) — forcing football-data, balldontlie, RapidAPI Flashscore...");
      fetchFootballDataBlocking();
      fetchNbaGames();
      fetchRapidApiFlashscore();
      Serial.printf("Scores: test fetch done — %d total entries.\n", scoreEntryCount);
      if (currentMode == MODE_SCORES) drawScoresView();
      return;
    case 'Q':
      if (currentMode == MODE_ANIMATION) Serial.println("MODE:ANIMATION");
      else if (currentMode == MODE_CLOCK) Serial.println("MODE:CLOCK");
      else if (currentMode == MODE_POMODORO) Serial.println("MODE:POMODORO");
      else if (currentMode == MODE_TERMINAL) Serial.println("MODE:TERMINAL");
      else if (currentMode == MODE_USAGE) Serial.println("MODE:USAGE");
      else if (currentMode == MODE_WEATHER) Serial.println("MODE:WEATHER");
      else if (currentMode == MODE_SING) Serial.println("MODE:SING");
      else if (currentMode == MODE_SCORES) Serial.println("MODE:SCORES");

      Serial.println(pomodoroActive ? "POMO:ON" : "POMO:OFF");
      Serial.printf("USAGE_DATA %d %d %d %d\n", usageSessionPct, usageWeeklyPct, usageSessionResetMin, usageWeeklyResetMin);
      
      for (int i = 0; i < 10; i++) {
        Serial.printf("ALARM_DATA %d %d %d %d %d %s\n",
                      i,
                      alarmsList[i].enabled ? 1 : 0,
                      alarmsList[i].hour,
                      alarmsList[i].minute,
                      alarmsList[i].days,
                      alarmsList[i].name[0] ? alarmsList[i].name : "-");
      }
      // Loc/condition are free text (may contain spaces) so they go last,
      // pipe-separated from each other — same convention as 'F' favorite
      // teams — after the fixed-count numeric fields.
      Serial.printf("WEATHER_DATA %d %d %d %d %s|%s\n",
                    wx.hasData ? 1 : 0, wx.tempC, wx.feelsC, wx.humidity,
                    wxLoc.c_str(), wxCondStr.c_str());
      return;
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
    case MODE_SING:      handleSingKey(c);      break;
    default: break;
  }
}

