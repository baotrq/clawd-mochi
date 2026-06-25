# CLAUDE.md

Guidance for working in this repo. The **firmware is the source of truth** — when this file, the README, or comments disagree with the code, trust `clawd_mochi/clawd_mochi.ino`.

## What this is

**Clawd Mochi** — a desk-companion toy. An **ESP32-C3 Super Mini** drives a **1.54" ST7789 240×240 SPI TFT** and shows animated pixel-art eyes / expressions, a clock, a Pomodoro timer, a fake terminal, and a logo reveal. It is an independent fan project themed on Anthropic's "Clawd" mascot (not affiliated with Anthropic).

There is **no app and no cloud**. The device is controlled two ways:
- **Serial @ 115200** — the primary, reliable control surface.
- **WiFi AP + tiny embedded web UI** — best-effort. This board's radio has tested as unreliable/defective, so Serial is the fallback that always works.

## Repo layout

```
clawd_mochi/
  clawd_mochi.ino          # THE firmware — single file, all logic + embedded web UI
  wifi_bare_test/
    wifi_bare_test.ino     # standalone sketch: minimal SoftAP to debug the flaky radio
models/                    # 3D-printable STLs (case + display-piece Clawd)
pics/                      # photos / renders / banner used by README
README.md                  # end-user build guide (see "Known mismatches" below)
```

There is intentionally **no host-side web/controller app** — a standalone web controller existed earlier and was removed. Do not reintroduce one unless asked; control goes through Serial or the embedded AP page.

## Build / flash

Arduino IDE 2.x. Settings (from README, matches this board):

| Setting | Value |
| --- | --- |
| Board | ESP32C3 Dev Module |
| USB CDC On Boot | **Enabled** (required for Serial over USB-C) |
| CPU Frequency | 160 MHz |
| Upload Speed | 921600 |

> **WiFi AP is RF-limited, not a config issue.** At boot the firmware reports the AP `started` with IP `192.168.4.1`, but the ESP32-C3 Super Mini's antenna design makes the AP weak/intermittent over the air on these boards — swapping to another board of the same model does not fix it. `setup()` already disables modem sleep and lowers TX power (`WIFI_POWER_8_5dBm`), which is the most-reported mitigation. Serial @115200 is the reliable control surface regardless. (CPU frequency does not fix this; an AP that "worked at 80 MHz" was a lucky intermittent boot.)

Libraries (Library Manager): **Adafruit GFX Library**, **Adafruit ST7735 and ST7789 Library**. WiFi/WebServer come with the ESP32 core.

Open `clawd_mochi/clawd_mochi.ino`, select the port, upload. There is no CLI build/test setup; verification is manual on hardware (or via the Serial Monitor — every command also prints there).

## Firmware architecture (`clawd_mochi.ino`)

Single `.ino`, organized top-to-bottom into commented sections:

- **Pins / config** — `TFT_CS=2, TFT_DC=3, TFT_RST=4, TFT_BLK=1`; SPI `SCK=8, MOSI=10`. AP `ClawdMochi` / `mochi1234` → `192.168.4.1`.
- **State** — a small view state machine: `VIEW_EYES_NORMAL / EYES_SQUISH / CODE (terminal) / CLOCK / INPUT / POMODORO`, plus `animSpeed` (1 slow … 3 fast), `dynamicMode`, `backlightOn`, `busy`.
- **Draw helpers** — `drawNormalEyes`, `drawSquishEyes`, `drawWinkEyes`, `drawDroopyEyes`, `drawEyesAsym`, `drawCodeView`, `drawClockView`, `drawPomodoro*`, `drawInputPrompt`. `speedMs(ms)` scales delays by `animSpeed`.
- **Subsystems** — clock (no RTC; runs off `millis()`, defaults 00:00, set via `t`; auto-pops every 30 min), Pomodoro (25 min work / 5 min break), alarm (`r`, screen flashes), terminal (15×8 char grid; `exit` to leave).
- **Animations** — many one-shot `anim*()` (blink, wink, sleepy, roll, etc.). `IDLE_ANIMS[]` is the pool that `dynamicMode` cycles randomly.
- **Logo** — `LOGO_TRIS` / `LOGO_SEGS` PROGMEM geometry; `animLogoReveal()` draws it stroke-by-stroke.
- **Dispatch** — `handleChar(char)` is the **single source of truth for commands**, shared by Serial and the web route so both behave identically.
- **Web** — `INDEX_HTML` PROGMEM page + routes (`routeRoot`, `routeCmd` which sends each char through `handleChar`, `routeNotFound`). `routeCmd` sets `Access-Control-Allow-Origin: *`.
- **`setup()` / `loop()`** — `loop()` pumps Serial, `server.handleClient()`, alarm/pomodoro/clock updates, and the dynamic-mode idle scheduler.

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

Note: the `e`–`x` single-shot expression commands were added in addition to the original set; each maps directly to an existing `anim*()` function and works over Serial as well as the web route.

## Conventions & constraints

- **Keep it a single `.ino`.** The README explicitly promises beginners one flat file to flash — do not split into multiple translation units / classes.
- **Route new commands through `handleChar`**, never duplicate logic in the web route — that's what keeps Serial and WiFi identical.
- **The embedded `INDEX_HTML` is PROGMEM and size-sensitive** — keep it minimal; it is not the place for a rich UI.
- **No RTC** — never assume wall-clock time; the clock is a `millis()` stopwatch until set.
- **Assume the WiFi radio may fail.** Anything important must remain operable over Serial. `wifi_bare_test.ino` exists solely to sanity-check the radio in isolation.
- Drive all animation delays through `speedMs()` so the speed setting keeps working.

## Known mismatches (don't trust blindly)

The **README is out of date vs. the firmware** in two places — believe the code:

- **Pins:** README lists CS/DC/RST/BL on GPIO 4/1/2/3; firmware uses **2/3/4/1**.
- **WiFi:** README says SSID `ClaWD-Mochi` / pass `clawd1234`; firmware uses **`ClawdMochi` / `mochi1234`**.

If you touch wiring or credentials, reconcile the README in the same change.
