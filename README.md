# ESP8266 LED Matrix Clock

32×8 WS2812 matrix clock running on Wemos D1 Mini, built on the
[maakbaas/esp8266-iot-framework](https://github.com/maakbaas/esp8266-iot-framework).

---

## Hardware

| Signal         | GPIO | D1 Mini pin | Notes |
|----------------|------|-------------|-------|
| BTN1 MODE      | 5    | D1          | INPUT_PULLUP, active LOW |
| BTN2 UP        | 14   | D5          | INPUT_PULLUP, active LOW (new in v1.005) |
| BTN3 DOWN      | 12   | D6          | INPUT_PULLUP, active LOW (new in v1.005) |
| Onboard LED    | 2    | D4          | Active LOW (built-in) |
| WS2812 data    | 4    | D2          | |
| Matrix         | 32 × 8 = 256 LEDs, column-snake order ||
| Power          | USB 5 V smartphone charger (220 V → 5 V) ||

> **Wiring BTN2 / BTN3**: connect one terminal to the GPIO pin, the other terminal to GND.
> The internal pull-up is enabled automatically; no external resistor is needed.

---

## Display layout

```
Col  0- 4  Hour tens digit
Col  5     space
Col  6-10  Hour units digit
Col 11     space
Col 12     colon ( : )
Col 13     space
Col 14-18  Minute tens digit
Col 19     space
Col 20-24  Minute units digit
Col 25     gap
Col 26     DATE column   (1 px per 7-day week, white, bottom→top)
Col 27     gap
Col 28     WEEKDAY column (Mon=1 px … Sun=7 px, cyan/orange)
Col 29     gap
Col 30     WEATHER column (4 px, colour = OWM condition)
Col 31     trailing pad
```

---

## Display modes

Press **BTN1 (MODE)** to cycle through four display modes:

| Mode  | Content | Colour |
|-------|---------|--------|
| CLOCK | HH:MM + info bars (default) | White + colour bars |
| IP    | Scrolling WiFi IP address, e.g. `IP 192.168.1.54` | Cyan |
| DATE  | Static date, e.g. `22 APR` | Green |
| TEMP  | Static temperature, e.g. `25.1C` | Yellow |

After WiFi connects at boot, IP mode is shown automatically for 8 seconds, then returns to CLOCK.

---

## Button reference

| Button | Press type | Action |
|--------|-----------|--------|
| **BTN1 MODE** (D1) | Single click | Cycle display mode: CLOCK → IP → DATE → TEMP → CLOCK |
| **BTN1 MODE** (D1) | Double click | Force NTP + weather refresh immediately |
| **BTN1 MODE** (D1) | Hold 1 s | Show IP address for 8 s |
| **BTN1 MODE** (D1) | Hold 5 s | Enter recovery mode |
| **BTN2 UP** (D5) | Single click | Brightness +1 step (Dim → Medium → Bright, wraps) |
| **BTN2 UP** (D5) | Hold 1 s | Force NTP + weather refresh |
| **BTN3 DOWN** (D6) | Single click | Brightness −1 step (Bright → Medium → Dim, wraps) |
| **BTN3 DOWN** (D6) | Hold 1 s | Toggle weather fetch on / off |

> **Note (boot window)**: Hold BTN1 during the first 3 seconds after power-on to enter recovery mode without waiting for the 5-second very-long-press.

---

## Weather colours

| Condition     | OWM code range | LED colour  |
|---------------|----------------|-------------|
| Thunderstorm  | 200–232        | Purple      |
| Drizzle       | 300–321        | Light blue  |
| Rain          | 500–531        | Blue        |
| Snow          | 600–622        | Icy blue    |
| Fog / mist    | 700–781        | Grey        |
| Clear sky     | 800            | Yellow      |
| Few/scattered clouds | 801–802 | Pale yellow |
| Broken / overcast | 803–804   | Grey        |

Full code list: <https://openweathermap.org/weather-conditions>

---

## Operating modes

### Normal mode
- Connects to saved WiFi (15 s timeout).
- If connected: syncs NTP, fetches weather. Refreshes display every 1 s (active) or
  30 s (idle), syncs NTP + weather every 60 min.
- If not connected: starts open AP **LED-CLOCK**, clock runs on manually set time.
- Onboard LED is **always OFF** in normal mode.

### Recovery mode
Entered when:
- BTN1 held ≥ 1 s during **boot window** (first 3 s)
- BTN1 held ≥ 5 s **at any time**
- Crash / watchdog reset detected at boot
- Weather fetch fails 5 consecutive times (network errors only; config errors like
  wrong API key or city name do **not** count)

Shows scrolling orange **RECOVERY** text. Starts open AP **LED-CLOCK** if WiFi
unavailable. Web GUI available at device IP or `192.168.4.1`.
**Exit by rebooting** (web button or power cycle).

### LED indicator
| State                       | Pattern         |
|-----------------------------|-----------------|
| Boot window / WiFi connect  | Fast blink 100 ms |
| Normal mode                 | **Always OFF**  |
| Recovery mode               | Slow blink 1 s  |
| OTA in progress             | Solid ON        |

---

## First-time setup

### 1. Prerequisites
- [VSCode](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- Node.js + npm (for the framework GUI build)

### 2. Install JS dependencies
```bash
cd .pio/libdeps/d1_mini/ESP8266\ IoT\ Framework
npm ci --legacy-peer-deps
```

### 3. Build and flash
```
PlatformIO: Build   (builds firmware + generates config/dashboard headers)
PlatformIO: Upload
```

### 4. First configuration
1. Connect to WiFi AP **LED-CLOCK** (open, no password).
2. Open browser → `192.168.4.1`
3. Go to **Configuration** and fill in:
   - **City** – e.g. `Hong Kong`
   - **OpenWeatherMap API Key** – free at [openweathermap.org](https://openweathermap.org/api)
   - **Timezone** – POSIX string, e.g. `HKT-8` or `CET-1CEST,M3.5.0,M10.5.0/3`
4. Go to **WiFi** and enter your network credentials.
5. Device reboots and connects automatically.
   After connect the IP address scrolls on the matrix for 8 seconds.

### Manual time (no WiFi)
If WiFi is unavailable, set **Manual Hour / Minute / Day / Month / Year /
Weekday** fields in the Configuration page (accessible at `192.168.4.1`).
The clock advances via `millis()` drift once saved.

### OTA firmware update
1. Build → `.pio/build/d1_mini/firmware.bin`
2. Open web GUI → **Update** → upload `firmware.bin`

---

## Serial monitor output (115200 baud)

### Boot sequence
```
================================
  LED Matrix Clock  v1.005
================================
[Sys] Chip ID   : XXXXXXXX
[Sys] SDK       : 3.0.x
[Sys] Flash     : 4096 KB
[Sys] Free heap : 45000 B
[Sys] CPU freq  : 160 MHz
[Sys] Reset     : Power On
[Boot] Buttons: MODE=GPIO5  UP=GPIO14  DOWN=GPIO12
[Boot] Config loaded
[Boot] Brightness level 1 (raw=128) | Orientation 0
[Boot] Boot window 3000ms — hold BTN1(D1) for recovery
[Boot] crash=0 rtcFlag=0 btnHold=0  =>  recovery=0
[WiFi] connected — IP 192.168.1.54
[NTP] timezone = HKT-8
[NTP] synced — 2025-04-22 12:00:00
[Weather] fetching city=Hong Kong
[Weather] OK  code=800  25.0°C  clear sky
[Boot] complete — heap=38000B
================================
```

### Heartbeat (every 60 s)
```
[Heartbeat] 12:01:00 | heap=37500B | ntp=1 | wx=800 25.0C [on] | mode=clock | bright=1 | orient=0 | wifi=192.168.1.54
```

Field meanings:

| Field | Meaning |
|-------|---------|
| `heap=37500B` | Free heap memory in bytes |
| `ntp=1` | NTP synced (1=yes, 0=no / using manual time) |
| `wx=800` | OpenWeatherMap condition code (800 = clear sky, see table above) |
| `25.0C` | Temperature in Celsius |
| `[on]` | Weather fetch enabled (`on`) or disabled (`off`) |
| `mode=clock` | Current display mode (`clock` / `ip` / `date` / `temp`) |
| `bright=1` | Brightness index (0=dim, 1=medium, 2=bright) |
| `orient=0` | Display orientation (0=normal) |
| `wifi=...` | WiFi IP address, `AP:192.168.4.1` in captive portal, `offline` if disconnected |

### Button events
```
[Button] MODE click — display mode: ip
[Button] UP click — brightness 2 (raw=255)
[Button] DOWN click — brightness 1 (raw=128)
[Button] DOWN 1 s — weather DISABLED
[Button] MODE double-click — force NTP + weather refresh
[Button] MODE 5 s — entering recovery
```

---

## Display orientation

The **Configuration** page exposes a **Display Orientation** selector:

| Value | Label              | Use case                         |
|-------|--------------------|----------------------------------|
| 0     | Normal             | Standard horizontal mount        |
| 1     | 90° CW             | Portrait mount (scaled)          |
| 2     | 180°               | Upside-down mount                |
| 3     | 270° CW            | Portrait mount, other way (scaled) |
| 4     | H-Flip             | Display wired from the right     |
| 5     | V-Flip             | Display wired from the bottom    |

---

## AI-assisted patch workflow

When an AI model (or any contributor) modifies files in this project,
**do not commit the full codebase**. Follow the zip-patch pattern that
`build.yml` handles automatically:

1. Identify the files that changed.
2. Package **only those changed files** into
   `zip_update/patches/update_NNN.zip`, preserving relative paths from
   the repo root (e.g. `src/main.cpp`, `gui/js/configuration.json`).
3. Push the zip to the repository (the rest of the code stays unchanged).
4. `build.yml` automatically:
   - Extracts the zip over the existing repo tree
   - Runs `apply.sh` if present in the zip
   - Commits the result with the CHANGELOG note
   - Builds and publishes the firmware release

### Rules for patch zips
| Rule | Detail |
|------|--------|
| Naming | `update_NNN.zip` — sequential number or date (`update_003.zip`, `update_20260418.zip`) |
| Contents | Changed files only — **no `.github/`** (blocked by CI) |
| Paths | Relative to repo root (`src/main.cpp` not `/home/user/src/main.cpp`) |
| CHANGELOG | Update `zip_update/CHANGELOG.md` — first line controls build (`#!build` or `#!no-build`) |
| Cleanup | Zip is deleted automatically after successful application |

### ⚠️ GitHub Actions workflow files cannot be self-updated

Any zip containing `.github/` is **rejected automatically** by the CI safety check.
If `build.yml` itself needs changing, commit it directly outside the patch mechanism.

---

## Project structure
```
led-clock/
├── platformio.ini
├── .gitignore
├── gui/js/
│   ├── configuration.json   ← edit to add/remove config fields
│   └── dashboard.json       ← edit to add/remove dashboard fields
├── zip_update/
│   ├── patches/             ← place update_NNN.zip here
│   ├── applied_patches.txt  ← auto-maintained log
│   └── CHANGELOG.md         ← release notes for next patch
└── src/
    ├── main.cpp             ← application entry point (state machine + display logic)
    ├── PinDefinitions.h     ← hardware pin assignments
    ├── LEDMatrixLayout.h    ← column-snake physical mapping
    ├── FontData.*           ← 5×7 pixel font (digits, A-Z, a-z, punctuation)
    ├── ButtonHandler.*      ← debounced button state machine (3 instances used)
    ├── StatusLED.*          ← onboard LED driver (unused directly; logic inline in main)
    ├── TimeManager.*        ← NTP + manual time wrapper (unused directly; logic inline in main)
    ├── WeatherService.*     ← OWM HTTP client (unused directly; logic inline in main)
    ├── DisplayManager.*     ← FastLED matrix driver (unused directly; logic inline in main)
    ├── ClockFace.*          ← digit renderer (unused directly; logic inline in main)
    └── InfoColumns.*        ← bar renderer (unused directly; logic inline in main)
```
