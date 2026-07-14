# CLAUDE.md

Guidance for working in this repo. The **firmware is the source of truth** — when this file, the README, or comments disagree with the code, trust `clawd_mochi/clawd_mochi.ino`.

## What this is

**Clawd Mochi** — a desk-companion toy. An **ESP32-C3 Super Mini** drives a **1.54" ST7789 240×240 SPI TFT** and shows animated pixel-art eyes / expressions, a clock, a Pomodoro timer, a fake terminal, and a logo reveal. It is an independent fan project themed on Anthropic's "Clawd" mascot (not affiliated with Anthropic).

There is **no app and no cloud**. The device has one control surface:
- **Serial @ 115200** — the only control surface, reliable always. The `web/` app (see below) also drives it, over the **Web Serial API** — that's still Serial over USB, not WiFi.

WiFi (`wifi_time.ino`) exists for exactly one thing: an optional background **NTP time sync**, purely a backup for the `t`/`T` Serial time-set commands — it is not a control surface and never was one on this board (an earlier WiFi AP + embedded web UI existed at one point and was removed; don't reintroduce one unless asked). This board's radio has tested as unreliable/defective, so the sync is written to retry quietly and never block anything if it fails.

## Repo layout

```
clawd_mochi/
  clawd_mochi.ino          # entry point — pins, globals, setup(), loop()
  helpers.ino              # speedMs(), setBacklight(), initColours()
  logo.ino                 # drawLogoFilled()
  eyes.ino                 # eye-drawing primitives (normal/squish/wink/droopy/asym)
  clock_view.ino           # clock screen + its per-loop redraw
  usage_view.ino           # Claude usage (5h/7d) screen
  weather_view.ino         # weather screen + animations
  input_prompt.ino         # numeric input overlay (set-clock/alarm/timer/pomo)
  pomodoro.ino             # Pomodoro timer logic + screens
  alarm.ino                # alarm arm/check/ring-flash
  timer.ino                # countdown timer arm/check/ring-flash
  terminal.ino             # fake terminal (+ cowsay easter egg)
  animations.ino           # one-shot anim*() expressions + idle-cycle pool
  mode_switching.ino       # switchMode() + handleChar() command dispatch
  wifi_time.ino            # optional STA WiFi connect + NTP background time sync
models/                    # 3D-printable STLs (case + display-piece Clawd)
pics/                      # photos / renders / banner used by README
README.md                  # end-user build guide (see "Known mismatches" below)
```

All `.ino` files above live together in the `clawd_mochi/` sketch folder. The Arduino IDE concatenates every `.ino` file in a sketch folder into one build before compiling, so this still flashes as a single program — it's split across files purely for readability/debugging, not a multi-sketch project. **All pins, globals, enums, structs, and PROGMEM data live in `clawd_mochi.ino`** (which the IDE always compiles first) so every other file can see them regardless of alphabetical order.

There is intentionally **no host-side web/controller app** — an earlier ESP32-hosted web UI existed and was removed. Do not reintroduce one unless asked; control goes through Serial only (the `web/` app included in this repo is a separately-hosted page that talks Web Serial, not something the device itself serves).

## Build / flash

Arduino IDE 2.x. Settings (from README, matches this board):

| Setting | Value |
| --- | --- |
| Board | ESP32C3 Dev Module |
| USB CDC On Boot | **Enabled** (required for Serial over USB-C) |
| CPU Frequency | 160 MHz |
| Upload Speed | 921600 |

> **This board's WiFi is RF-limited, not a config issue.** The ESP32-C3 Super Mini's antenna design makes WiFi weak/intermittent over the air on these boards — swapping to another board of the same model does not fix it. `wifi_time.ino` already disables modem sleep and lowers TX power (`WIFI_POWER_8_5dBm`), the most-reported mitigation, and treats a failed/slow connection as normal (retries every 5 min, never blocks). Serial @115200 is the reliable control surface regardless — WiFi only ever affects whether the clock has real time or not. (CPU frequency does not fix radio flakiness.)

Libraries (Library Manager): **Adafruit GFX Library**, **Adafruit ST7735 and ST7789 Library**. WiFi comes with the ESP32 core (used by `wifi_time.ino` only — no WebServer/HTTP anywhere in this sketch).

Open `clawd_mochi/clawd_mochi.ino`, select the port, upload. There is no CLI build/test setup; verification is manual on hardware (or via the Serial Monitor — every command also prints there).

## Firmware architecture (`clawd_mochi/` sketch)

One sketch, split across the `.ino` files listed above (the Arduino IDE builds them as a single program — see the repo layout note):

- **Pins / config** — `TFT_CS=2, TFT_DC=3, TFT_RST=4, TFT_BLK=1`; SPI `SCK=8, MOSI=10`. `STA_SSID`/`STA_PASS` (optional, blank by default) for background NTP time sync — live in `secrets.h` (gitignored; `#include`d by `clawd_mochi.ino`), not in the tracked sketch. Copy `secrets.h.example` → `secrets.h` before building; it must exist to compile.
- **State** — a small view state machine: `VIEW_EYES_NORMAL / EYES_SQUISH / CODE (terminal) / CLOCK / INPUT / POMODORO`, plus `animSpeed` (1 slow … 3 fast), `dynamicMode`, `backlightOn`, `busy`.
- **Draw helpers** — `drawNormalEyes`, `drawSquishEyes`, `drawWinkEyes`, `drawDroopyEyes`, `drawEyesAsym`, `drawCodeView`, `drawClockView`, `drawPomodoro*`, `drawInputPrompt`. `speedMs(ms)` scales delays by `animSpeed`.
- **Subsystems** — clock (no RTC; runs off `millis()`, defaults 00:00, set via `t`/`T` or automatically by WiFi+NTP once it syncs; auto-pops every 30 min), Pomodoro (25 min work / 5 min break), alarm (`r`, screen flashes), terminal (15×8 char grid; `exit` to leave).
- **Animations** — many one-shot `anim*()` (blink, wink, sleepy, roll, etc.). `IDLE_ANIMS[]` is the pool that `dynamicMode` cycles randomly.
- **Logo** — `LOGO_TRIS` / `LOGO_SEGS` PROGMEM geometry; `animLogoReveal()` draws it stroke-by-stroke.
- **Dispatch** — `handleChar(char)` is the **single source of truth for commands**, called from the Serial read loop in `loop()`. There is no web route — the `web/` app sends the same characters over Web Serial.
- **WiFi/NTP** (`wifi_time.ino`) — a small state machine (`updateWifiTime()`, stepped once per `loop()`) that connects STA, calls `configTime()`, and polls `getLocalTime()` until it succeeds; sets the shared `timeSynced` flag on success and otherwise retries every 5 min. Entirely separate from command dispatch — it only ever affects wall-clock time.
- **`setup()` / `loop()`** — `loop()` pumps Serial, `updateWifiTime()`, alarm/pomodoro/clock updates, and the dynamic-mode idle scheduler.

### Command set (single chars → `handleChar`)

```
w normal eyes   s squish eyes   d terminal   a logo reveal
m dynamic mode (toggle idle-animation cycling)
b backlight toggle      1/2/3 speed slow/normal/fast
c show clock    t set time (then type HHMM + Enter)
p pomodoro start/stop   r set alarm (then type minutes + Enter)
single-shot expressions:
  e blink  f double-blink  g look-around  h wink  i sleepy
  j surprised  k squint  l nod  n shake  o roll
  u cross-eyed  v tilt-confused  x excited
in terminal: type "exit" + Enter to leave
```

Note: the `e`–`x` single-shot expression commands were added in addition to the original set; each maps directly to an existing `anim*()` function and works over Serial (there is no other route — see above).

Note: `D<index>` + Enter (e.g. `D24\n`) is a debug/validation-only global command — plays `IDLE_ANIMS[index]` (`animations.ino`) on demand and echoes its name from the parallel `IDLE_ANIM_NAMES[]`, so a newly added idle animation can be checked over the Serial Monitor without waiting for `dynamicMode`'s random cycle to land on it.

## Conventions & constraints

- **Keep it one sketch (the `clawd_mochi/` folder), no classes/translation units.** The code is split across multiple `.ino` files for readability, but the Arduino IDE concatenates them into a single build — there is still only one thing to open and flash (`clawd_mochi.ino`), matching the README's promise of an easy beginner flash. Don't introduce `.h`/`.cpp` files or OOP-style modularization; new code should be a new `.ino` file in the same folder, or added to an existing one. **Exception:** `secrets.h` (gitignored, `#include`d for `STA_SSID`/`STA_PASS`) — a plain data header for credentials, not logic, so it doesn't fall under this rule. If more secrets/API keys are ever needed, add `#define`s to that same file rather than creating new headers.
- **All globals live in `clawd_mochi.ino`.** It's always compiled first, so every other file can see its pins/enums/structs/state without an include. Don't declare new global state in a non-main file.
- **Route new commands through `handleChar`**, never add a second command path — Serial is the only one, and the `web/` app relies on sending the exact same characters.
- **No RTC** — never assume wall-clock time is accurate; the clock is a `millis()` stopwatch by default. `wifi_time.ino` can fill in real time via NTP, but only ever as a best-effort backup — code must keep working correctly if `timeSynced` stays false forever (e.g. no `STA_SSID` set, or the radio never connects).
- **Assume the WiFi radio may fail.** Anything important (all control) must remain operable over Serial regardless of WiFi state. Don't add new WiFi-dependent control paths — WiFi in this sketch is scoped to `wifi_time.ino`'s NTP sync only.
- Drive all animation delays through `speedMs()` so the speed setting keeps working.

## Known mismatches (don't trust blindly)

The **README is out of date vs. the firmware** in one place — believe the code:

- **Pins:** README lists CS/DC/RST/BL on GPIO 4/1/2/3; firmware uses **2/3/4/1**.

If you touch wiring or credentials, reconcile the README in the same change.
