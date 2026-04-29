# ESP8266 LED Matrix Clock

32×8 WS2812 matrix clock running on Wemos D1 Mini, built on the
[maakbaas/esp8266-iot-framework](https://github.com/maakbaas/esp8266-iot-framework).

---

## Hardware

| Signal         | GPIO | D1 Mini pin | Notes |
|----------------|------|-------------|-------|
| BTN1 MODE      | 5    | D1          | INPUT_PULLUP, active LOW |
| BTN2 UP        | 14   | D5          | INPUT_PULLUP, active LOW |
| BTN3 DOWN      | 12   | D6          | INPUT_PULLUP, active LOW |
| BTN4 CONFIRM   | 13   | D7          | INPUT_PULLUP, active LOW ← **new in v1.007** |
| Onboard LED    | 2    | D4          | Active LOW (built-in) |
| WS2812 data    | 4    | D2          | |
| Matrix         | 32 × 8 = 256 LEDs, column-snake order ||
| Power          | USB 5 V smartphone charger ||

> **Wiring BTN2 / BTN3 / BTN4**: connect one terminal to the GPIO pin, the other terminal to GND.
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

Press **BTN1 (MODE)** to cycle through four display modes (LED refreshes immediately on switch):

| Mode  | Content | Colour |
|-------|---------|--------|
| CLOCK | HH:MM + info bars (default) | White + colour bars |
| DATE  | Static date, e.g. `22APR` | Green |
| TEMP  | Static temperature, e.g. `25.1C` | Yellow |
| IP    | Scrolling WiFi IP address | Cyan |

After WiFi connects at boot, IP mode is shown automatically for 8 seconds, then returns to CLOCK.

---

## Button reference

| Button | Press type | Normal mode | Settings mode | Recovery mode |
|--------|-----------|-------------|---------------|---------------|
| **BTN1 MODE** (D1) | Single click | Cycle display mode | Next setting item | — |
| **BTN1 MODE** (D1) | Hold 3 s | Enter settings mode | Save & exit | Clear flag & reboot |
| **BTN1 MODE** (D1) | Hold 8 s | Enter recovery mode | Save + enter recovery | — |
| **BTN2 UP** (D5) | Single click | Brightness +1 step | Value +1 | — |
| **BTN2 UP** (D5) | Hold 3 s | Force NTP + weather refresh | Value +5 (fast) | — |
| **BTN3 DOWN** (D6) | Single click | Brightness −1 step | Value −1 | — |
| **BTN3 DOWN** (D6) | Hold 3 s | Toggle weather on/off | Value −5 (fast) | — |
| **BTN4 CONFIRM** (D7) | Single click | Show IP for 8 s | **Save** & exit | Clear flag & reboot |
| **BTN4 CONFIRM** (D7) | Hold 3 s | — | **Cancel** (restore snapshot) | — |

> **Boot window**: Hold BTN1 during the first 3 seconds after power-on to enter recovery mode.

---

## Settings mode

Enter by holding **BTN1 MODE for 3 seconds**. The LED matrix shows the current setting in cyan.

**Navigation:**
- **BTN1 click** — move to next setting
- **BTN2 UP click / hold** — increment value (+1 / +5)
- **BTN3 DOWN click / hold** — decrement value (−1 / −5)
- **BTN4 click** — **save** all changes and exit
- **BTN4 hold 3 s** — **cancel** (restores the snapshot taken when settings was entered; nothing is saved)
- **BTN1 hold 3 s** — save and exit
- Auto-save after 30 seconds of inactivity

**Available settings** (NTP-synced time/date items are hidden when clock is synced):

| LED label | Setting | Range |
|-----------|---------|-------|
| `H:` | Manual hour | 0–23 |
| `m:` | Manual minute | 0–59 |
| `d:` | Manual day | 1–31 |
| `Mo:` | Manual month | 1–12 |
| `Y:` | Manual year | 2024–2099 |
| `Wd:` | Manual weekday | Mon–Sun |
| `TZ:` | Timezone preset | 18 POSIX presets |
| `RO:` | Rotation | N / 90 / 180 / 270 |
| `FL:` | Flip | N / H / V |
| `SP:` | Scroll speed (ms/col) | 30–200 (step 5) |
| `DM:` | Dim PWM value | 1–200 (step 5) |
| `MD:` | Medium PWM value | 1–255 (step 5) |
| `BR:` | Bright PWM value | 1–255 (step 5) |
| `Wx:` | Weather on/off | ON / OFF |
| `Wi:` | WiFi on/off | ON / OFF |

---

## Display orientation

Rotation and flip are **independent** and **combinable**:

**Rotation** (`rotation` config field):
| Value | Label | Description |
|-------|-------|-------------|
| 0 | Normal | Standard horizontal mount |
| 1 | 90° CW | Portrait (scaled) |
| 2 | 180° | Upside-down |
| 3 | 270° CW | Portrait, other way |

**Flip** (`flip` config field):
| Value | Label | Description |
|-------|-------|-------------|
| 0 | None | No mirroring |
| 1 | H-Flip | Mirror left-right |
| 2 | V-Flip | Mirror top-bottom |

Both can be set independently via the web Configuration page or the on-device settings mode.
Examples: 180° + H-Flip = same visual as V-Flip; 90° CW + H-Flip = 90° CCW.

---

## Weather colours

| Condition     | OWM code range | LED colour  |
|---------------|----------------|-------------|
| Thunderstorm  | 200–299        | Purple      |
| Drizzle       | 300–399        | Light blue  |
| Rain          | 500–599        | Blue        |
| Snow          | 600–699        | Icy blue    |
| Fog / mist    | 700–799        | Grey        |
| Clear sky     | 800            | Yellow      |
| Few/scattered clouds | 801–802 | Pale yellow |
| Broken / overcast | 803–804   | Grey        |

Full code list: <https://openweathermap.org/weather-conditions>

---

## Scroll speed

The **scroll speed** (ms per column) controls how fast all scrolling text moves:
- IP address display
- RECOVERY mode text
- Settings items that are wider than the matrix

Default: **80 ms/column** (~12 columns/second).  
Range: 30 ms (fast) – 200 ms (slow). Adjustable via web Configuration or on-device settings (`SP:`).

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
- BTN1 held during boot window (first 3 s)
- BTN1 held ≥ 8 s at any time
- Crash / watchdog reset detected at boot
- Weather fetch fails 5 consecutive times (network errors only)

Shows scrolling orange **RECOVERY** text. Web GUI available at device IP or `192.168.4.1`.
**Exit**: press BTN4 (CONFIRM) or hold BTN1 for 3 s → clears RTC flag and reboots.

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
  LED Matrix Clock  v1.007
================================
[Sys] Reset:Power On  Heap:45000B  CPU:160MHz
[Boot] BTN MODE=GPIO5 UP=GPIO14 DOWN=GPIO12 CONFIRM=GPIO13
[Boot] bright=0(raw=1) rot=0 flip=0 spd=80 wx=1 wifi=1
[Boot] window 3000ms — hold BTN1(MODE) for recovery
[Boot] crash=0 rtc=0 btn=0 → recovery=0
[WiFi] IP 192.168.1.54
[NTP] tz=HKT-8
[NTP] synced 2025-04-22 12:00:00
[Weather] fetching city=Hong Kong
[Weather] OK code=800 25.0°C clear sky
[Boot] done  heap=38000B
================================
```

### Heartbeat (every 60 s)
```
[Heart] 12:01:00 heap=37500 ntp=1 wx=800 25.0C [on] app=NRM disp=clock bri=0 rot=0 flip=0 spd=80 wifi=192.168.1.54
```

### Button events
```
[Btn] MODE click → date
[Btn] MODE click → temp
[Btn] MODE click → ip
[Btn] CONFIRM → show IP
[Btn] UP → bright 1(raw=128)
[Btn] DOWN → bright 0(raw=1)
[Btn] DOWN 3 s → weather OFF
[Btn] MODE 3 s → settings
[Settings] 1/13 TZ:HKT+8
[Btn] CONFIRM click → save
[Settings] saved — returning to normal
[Btn] CONFIRM 3 s → cancel
[Settings] cancelled — config restored (not saved)
[Btn] MODE 8 s → recovery
```

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
| Naming | `update_NNN.zip` — sequential number or date |
| Contents | Changed files only — **no `.github/`** (blocked by CI) |
| Paths | Relative to repo root (`src/main.cpp` not `/home/user/src/main.cpp`) |
| CHANGELOG | Update `zip_update/CHANGELOG.md` — first line controls build (`#!build` or `#!no-build`) |
| Cleanup | Zip is deleted automatically after successful application |

---

## Project structure
```
led-clock/
├── platformio.ini
├── .gitignore
├── gui/js/
│   ├── configuration.json   ← rotation/flip/scrollSpeed fields
│   └── dashboard.json       ← live clock/weather/system display
├── zip_update/
│   ├── patches/             ← place update_NNN.zip here
│   ├── applied_patches.txt  ← auto-maintained log
│   └── CHANGELOG.md         ← release notes for next patch
└── src/
    ├── main.cpp             ← setup() + loop() + global state
    ├── AppState.h           ← enums, constants, extern globals
    ├── PinDefinitions.h     ← GPIO assignments (4 buttons + LED + matrix)
    ├── BtnSimple.h          ← header-only 3-level button state machine
    ├── LEDMatrixLayout.h    ← column-snake physical mapping
    ├── FontData.*           ← 5×7 pixel font
    ├── ClockDisplay.*       ← display primitives + face renderers
    ├── SettingsMode.*       ← on-device settings editor (snapshot/cancel)
    ├── WeatherFetch.*       ← OWM HTTP client + recovery trigger
    ├── ButtonHandler.*      ← legacy (not used directly)
    ├── StatusLED.*          ← legacy (not used directly)
    ├── TimeManager.*        ← legacy (not used directly)
    ├── WeatherService.*     ← legacy (not used directly)
    ├── DisplayManager.*     ← legacy (not used directly)
    ├── ClockFace.*          ← legacy (not used directly)
    └── InfoColumns.*        ← legacy (not used directly)
```
