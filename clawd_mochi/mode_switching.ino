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
    case MODE_SING:      handleSingKey(c);      break;
    default: break;
  }
}

