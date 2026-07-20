<!-- LOGO -->
<p align="center">
  <img src="pics/clawd_print_face.jpg" alt="Clawd Mochi — 3D-printed desk companion" width="360"/>
  <img src="pics/clawd_print_logo.jpg" alt="Clawd Mochi showing the Anthropic logo" width="360"/>
</p>

# Clawd Mochi — Extended Edition 🦀🤖

A physical desk companion built around **Clawd**, the pixel-crab mascot of Claude Code. An **ESP32-C3** drives a 1.54" color TFT that shows animated expressions, a clock, a Pomodoro timer, a fake terminal, and a glanceable **Claude usage** meter — all driven from a browser, with no app and no cloud.

> 🍴 **This is a community contribution to the original [Clawd Mochi](https://github.com/yousifamanuel/clawd-mochi) by [@yousifamanuel](https://github.com/yousifamanuel).**
> The original delivers the hardware, the case, and the first firmware. **This fork focuses on two things:**
> 1. **More on-device functionality** — extra modes, a richer expression set, multi-alarm + background Pomodoro, and a live Claude-usage display.
> 2. **A dedicated web controller** — a pixel-art "room" you click to drive the device, **inspired by [Claude FM](https://claude.fm)**'s cozy lofi-focus aesthetic.

---

> ⚠️ Independent fan project. Not affiliated with, sponsored by, or endorsed by Anthropic. "Claude" and "Clawd" are trademarks of Anthropic.

---

<p align="center">
  <img src="pics/clawd_print_pomodoro.jpg" alt="Clawd Mochi running the Pomodoro focus timer" width="760"/>
</p>

---

## ✨ What this fork adds

**Firmware** ([`clawd_mochi/clawd_mochi.ino`](clawd_mochi/clawd_mochi.ino))
- **Five modes** instead of just expressions: **Animation**, **Clock**, **Pomodoro**, **Terminal**, and a new **Usage** mode.
- **Claude usage meter** — the web app pushes your session / weekly usage; the device shows it and auto-pops a glanceable nudge as you approach each 10% mark and 95%.
- **Background Pomodoro** — keeps ticking after you switch modes; set focus/break in one shot.
- **Multi-alarm + clock auto-pop** — clock briefly pops on each quarter-hour, then returns to idle animations.
- **Expanded expression set** — blink, double-blink, look-around, wink, sleepy, surprised, squint, nod, shake, roll, cross-eyed, confused, excited, plus a line-by-line logo reveal.
- **Silent sync commands** — time and usage are synced without disturbing whatever's on screen.

**Web controller** ([`web/`](web/)) — *inspired by Claude FM*
- A pixel-art office "room" with **clickable hotspots** (painting, shelf lock, fan, screen, sleeping cat) that each switch a device mode — the cozy, ambient feel of a lofi focus room rather than a wall of buttons.
- Talks to the device over **Web Serial (USB)** — works straight from the hosted HTTPS page, no app install.
- Full control panel for every expression/mode, alarms, timer, and terminal.
- Deployed to **GitHub Pages** via [`.github/workflows/deploy.yml`](.github/workflows/deploy.yml).

**Mascot sprite pipeline** ([`tools/`](tools/))
- Scripts that cut the orange mascot out of a captured video, sort the frames into pose sets (typing / get off chair / walking / jumping up), and build looping transparent GIFs — see [`mascot_frames/`](mascot_frames/).

---

## 🕹️ The web controller

> **Live app:** https://baotrq.github.io/clawd-mochi/ *(enable GitHub Pages on your fork to publish your own)*

<p align="center">
  <img src="pics/web_controller_room.png" alt="Pixel-art web controller room with clickable hotspots" width="700"/>
</p>

Instead of a form of buttons, the controller is a **room you interact with** — click an object to put Clawd into the matching mode:

| Hotspot           | Mode      | What it does                          |
| ----------------- | --------- | ------------------------------------- |
| 🖼️ Painting       | Animation | Expressions / idle animation cycling  |
| 🔒 Shelf lock     | Clock     | Clock, alarms, and a timer            |
| 🌀 Spinning fan   | Pomodoro  | Focus / break timer                   |
| 🖥️ Computer screen | Terminal  | Type freely on the device's screen    |
| 🐱 Sleeping cat   | Usage     | Live Claude usage meter               |
| ⚽ Scores         | Scores    | Live sports scores (see [command reference](#-firmware-command-reference)) |

**How control works:** the page uses the **Web Serial API** (Chrome / Edge / Opera) to talk to the ESP32-C3 over the **USB-C cable** at 115200 baud. Click **Connect**, pick the port, and every hotspot/button sends the same single-character commands the firmware understands. Because Web Serial is a secure-context API, this works directly from the HTTPS site — no mixed-content issues, and it's immune to the C3's flaky radio.

**Calibrating hotspots:** add `?calibrate` to the URL to drag the hotspots over your own room image; defaults live in `DEFAULT_HOTSPOTS` in [`web/src/App.jsx`](web/src/App.jsx) and per-browser tweaks are saved to local storage.

### Run the web app locally

```bash
cd web
npm install
npm run dev     # local dev server
npm run build   # production build into web/dist
```

---

## 🎛️ Firmware command reference

Every command is a single character, handled by `handleChar()` so **Serial and the web route behave identically**. Control over **Serial Monitor @ 115200** always works.

**Modes** (switch anytime): `1` Animation · `2` Clock · `3` Pomodoro · `4` Terminal · `5` Usage · `8` Scores

**Global:** `b` toggle backlight · `-` / `=` speed down / up

**Animation:** `w` normal · `s` squish · `z` logo reveal · `m` toggle dynamic idle cycling
Expressions: `e` blink · `f` double-blink · `g` look-around · `h` wink · `i` sleepy · `j` surprised · `k` squint · `l` nod · `n` shake · `o` roll · `u` cross-eyed · `v` tilt-confused · `x` excited

**Clock:** `t` set time (`HHMM`) · `r` set alarm (`HHMM`) · `y` set timer (seconds)
Alarm and timer take an optional name after a space — e.g. `0730 Wake up`, `300 Tea` — shown on the flashing ring screen instead of `ALARM!` / `TIMER!` (no name → default).
**Pomodoro:** `p` start/stop (runs in background) · `P` set + start (`MMSSB` digits)
**Terminal:** type freely; `exit` + Enter leaves
**Sync (sent by the web app, silent):** `T` set clock · `U` push usage stats · `F` set favorite teams (`F<team1>|<team2>|...`)
**Debug:** `D<index>` play an idle animation on demand · `Z` force an immediate Scores fetch, bypassing the 5min/2h timers (useful for checking each score API works without waiting on a real match — see Serial Monitor for per-source HTTP results)
**Scores (`8`):** fetches all 12 football-data.org free-tier competitions (World Cup, Champions League, Premier League, La Liga, Bundesliga, Serie A, Ligue 1, Euro Championship, Brasileirão, Primeira Liga, Eredivisie, Championship — fetched biggest-league-first so a busy day's shared slot budget favors them), plus NBA (balldontlie.io) and Europa League + VBA/Vietnamese Basketball (RapidAPI Flashscore) directly over WiFi, rotating one card at a time. Football-data/NBA fetches cover live matches AND the last 3 days of finished results (not just whatever happens to be live at the exact fetch moment — nearly always nothing, especially out of season), skipping scheduled/postponed games. Europa League/VBA (RapidAPI Flashscore) is live-only for now — no recent-results lookback yet, since that'd need a separate, still-unverified endpoint. A live game involving a team from the `F` list is pinned instead of rotated past. Requires `FOOTBALL_DATA_API_KEY` / `BALLDONTLIE_API_KEY` / `RAPIDAPI_FLASHSCORE_KEY` in `secrets.h` — any left blank just skips that source. The RapidAPI key is polled far less often (every 2h vs 5min) since its free plan caps out at 500 requests/month, shared across Europa League + VBA.

> No RTC — the clock runs off `millis()` and defaults to `00:00` until set (manually over Serial, or automatically via WiFi+NTP if configured — see [Connectivity](#-connectivity)).

---

## 🔩 Hardware

### Parts (~$7–8)

| Part                | Spec                         | ~Price |
| ------------------- | ---------------------------- | ------ |
| ESP32-C3 Super Mini | MCU with WiFi                | ~$2.50 |
| ST7789 1.54" TFT    | 240×240 SPI color display    | ~$3.00 |
| 8 jumper wires      | 8–10 cm Dupont               | ~$0.50 |
| 2× M2×6mm screws    | mount the display bezel      | ~$0.10 |
| Double-sided tape   | secure components            | ~$0.10 |
| USB-C cable         | power **and** Web Serial     | —      |
| 3D-printed case     | PLA / PETG, ~30 g            | ~$0.50 |

### Wiring

> ⚠️ VCC to **3.3 V only**, never 5 V. SPI uses GPIO 8 (SCK) and 10 (MOSI). Pin assignments below match the firmware (`clawd_mochi.ino`) — the authoritative source.

| Display pin | ESP32-C3 GPIO  | Suggested wire |
| ----------- | -------------- | -------------- |
| VCC         | 3V3            | Red            |
| GND         | GND            | Black          |
| SDA         | GPIO 10 (MOSI) | Orange         |
| SCL         | GPIO 8 (SCK)   | Green          |
| RES / RST   | GPIO 4         | Purple         |
| DC          | GPIO 3         | Blue           |
| CS          | GPIO 2         | White          |
| BL          | GPIO 1         | Yellow         |

---

## ⚙️ Build & flash

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software).
2. **Boards Manager URL** → `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`, then install **esp32 by Espressif Systems**.
3. **Library Manager** → install **Adafruit GFX Library**, **Adafruit ST7735 and ST7789 Library**, and **ArduinoJson** (needed by `scores.ino`).
4. **Tools** settings:

   | Setting         | Value                |
   | --------------- | -------------------- |
   | Board           | ESP32C3 Dev Module   |
   | USB CDC On Boot | **Enabled** (required for USB Serial) |
   | CPU Frequency   | 160 MHz              |
   | Upload Speed    | 921600               |

5. Open [`clawd_mochi/clawd_mochi.ino`](clawd_mochi/clawd_mochi.ino), select the port, and **Upload**.

---

## 📶 Connectivity

**Control** is always over **USB Web Serial / Serial @ 115200** — that's how the web controller talks to the device, and it's the only control surface. There's no embedded control page and no WiFi AP.

**WiFi** backs four things, all optional and all best-effort: a background **NTP time sync** (backup for the `t`/`T` Serial time-set commands, `wifi_time.ino`), a direct HTTPS pull of Claude usage stats (`wifi_sync.ino`), direct HTTPS pulls of live sports scores for Scores mode (`scores.ino`, see [Firmware command reference](#-firmware-command-reference)), and a direct HTTPS pull of current weather (`weather.ino`) — this last one is a fallback only, since the web app pushing weather over Serial (`W`) is the primary, instant path; `weather.ino` just fills in when the browser isn't connected. None of these are control surfaces — Serial remains the only way to command the device. Copy [`clawd_mochi/secrets.h.example`](clawd_mochi/secrets.h.example) to `clawd_mochi/secrets.h` and fill in `STA_SSID` / `STA_PASS` to join your home WiFi (2.4 GHz), plus whichever API keys you want (`CLAUDE_ACCESS_TOKEN`, `FOOTBALL_DATA_API_KEY`, `BALLDONTLIE_API_KEY`, `RAPIDAPI_FLASHSCORE_KEY`, `OPENWEATHER_API_KEY`). `secrets.h` is gitignored so your real credentials never get committed. `clawd_mochi.ino` `#include`s `secrets.h`, so **the file must exist to compile** — copy the example even if you're leaving every field blank. Any blank field just skips that feature; the clock falls back to `millis()`-based/manual-Serial time, and each score/weather source is silently skipped.

> ⚠️ **This board's radio is flaky.** The ESP32-C3 Super Mini has a known antenna limitation that makes WiFi weak/intermittent over the air — not a setting you can fix (CPU frequency does **not** change this). The NTP sync is written to retry quietly in the background and never blocks anything if it fails; control always works over **USB / Serial**, WiFi or not.

---

## 🖼️ Mascot sprite pipeline

The [`tools/`](tools/) scripts turn a captured animation video into clean, transparent sprite sets:

| Script | Purpose |
| ------ | ------- |
| [`clawd_frames_process.py`](tools/clawd_frames_process.py) | Color-key background removal + contact sheets |
| [`sort_frames.py`](tools/sort_frames.py) | Sort frames into pose folders (per-category stride) |
| [`make_gifs.py`](tools/make_gifs.py) | One looping transparent GIF per pose |

Outputs land in [`mascot_frames/`](mascot_frames/) — cutouts, categorized poses (typing / get_off_chair / walking / jumping_up), and GIFs. Requires Python with Pillow, NumPy, and SciPy.

---

## 🧱 3D case

Print files and settings are unchanged from the original — see [`models/`](models/), or download from MakerWorld:
- Case: https://makerworld.com/en/models/2559505-clawd-mochi-physical-claude-code-mascot
- Display-piece Clawd: https://makerworld.com/en/models/2576503-clawd-claude-code-mascot

| Setting      | Value                               |
| ------------ | ----------------------------------- |
| Material     | PLA or PETG                         |
| Layer height | 0.15–0.20 mm                        |
| Infill       | 15% gyroid                          |
| Supports     | Yes — for the display-window overhang |
| Orientation  | Face-down, flat back on plate       |

---

## 🙏 Credits

- **Original project:** [Clawd Mochi](https://github.com/yousifamanuel/clawd-mochi) by [@yousifamanuel](https://github.com/yousifamanuel) — hardware design, 3D case, and the first firmware.
- **This fork:** expanded firmware (modes, usage meter, background Pomodoro, multi-alarm, extra expressions), the web controller, and the mascot sprite pipeline.
- **Inspiration:** the web controller's cozy, ambient feel takes after **[Claude FM](https://claude.fm)**.
- Support / build photos: [Instagram @clawd.mochi](https://instagram.com/clawd.mochi).

The firmware is split across several `.ino` files in `clawd_mochi/` for readability, but the Arduino IDE builds a whole sketch folder as one program — open `clawd_mochi/clawd_mochi.ino`, select your port, and Upload, same as always. PRs welcome.

## License

MIT — see [LICENSE](LICENSE). 3D models and media assets are **CC BY-NC-SA 4.0**.
