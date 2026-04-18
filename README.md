# ESP8266 LED Matrix Clock

32×8 WS2812 matrix clock running on Wemos D1 Mini, built on the
[maakbaas/esp8266-iot-framework](https://github.com/maakbaas/esp8266-iot-framework).

---

## Hardware

| Signal         | GPIO | D1 Mini pin |
|----------------|------|-------------|
| Button         | 5    | D1          |
| Onboard LED    | 2    | D4 (active LOW) |
| WS2812 data    | 4    | D2          |
| Matrix         | 32 × 8 = 256 LEDs, column-snake order ||
| Power          | USB 5 V smartphone charger (220 V → 5 V) ||

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
Col 26     DATE column   (1 px per 7 days, white)
Col 27     gap
Col 28     WEEKDAY column (Mon=1 px … Sun=7 px, cyan/orange)
Col 29     gap
Col 30     WEATHER column (4 px, colour = OWM condition)
Col 31     trailing pad
```

### Weather colours
| Condition     | Colour  |
|---------------|---------|
| Clear         | Yellow  |
| Few clouds    | Pale yellow |
| Overcast      | Grey    |
| Drizzle       | Light blue |
| Rain          | Blue    |
| Thunderstorm  | Purple  |
| Snow          | Cyan    |
| Fog / mist    | Dim grey |

---

## Operating modes

### Normal mode
- Connects to saved WiFi (15 s timeout).
- If connected: syncs NTP, fetches weather. Refreshes display every 30 s,
  syncs NTP + weather every 60 min.
- If not connected: starts open AP **LED-CLOCK**, clock runs on manually set
  time. Retries WiFi every 60 min automatically.
- Onboard LED is **always OFF** in normal mode.

### Recovery mode
Entered when:
- Button held ≥ 1 s **at any time** (boot window OR during operation)
- Crash / watchdog reset detected at boot
- Weather fetch fails 5 consecutive times

Shows scrolling orange **RECOVERY** text. Tries to connect to saved WiFi;
starts open AP **LED-CLOCK** if it fails. Web GUI available at device IP or
`192.168.4.1`. **Exit by rebooting** (web reboot button or power cycle).

### LED indicator
| State                       | Pattern         |
|-----------------------------|-----------------|
| Boot window / WiFi connect  | Fast blink 100 ms |
| Normal mode                 | **Always OFF**  |
| Recovery mode               | Slow blink 1 s  |
| OTA in progress             | Solid ON        |

### Button actions (normal mode)
| Press               | Action                           |
|---------------------|----------------------------------|
| Single click        | Cycle brightness (Dim → Med → Bright → Dim) |
| Double click        | Force NTP + weather refresh      |
| Hold ≥ 1 s          | Enter recovery mode              |
| Hold ≥ 5 s          | Reboot                           |

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

> **Note:** 90° and 270° apply a scaled transform to fill the 32×8 display.
> Because the display is not square, the content will appear geometrically
> compressed; these modes are most useful for custom pixel art rather than
> time display.

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

### Manual time (no WiFi)
If WiFi is unavailable, set **Manual Hour / Minute / Day / Month / Year /
Weekday** fields in the Configuration page (accessible at `192.168.4.1`).
The clock advances via `millis()` drift once saved.

### OTA firmware update
1. Build → `.pio/build/d1_mini/firmware.bin`
2. Open web GUI → **Update** → upload `firmware.bin`

---

## Serial monitor output

Connect at **115200 baud** to see:

```
================================
  LED Matrix Clock  v1.2.0
================================
[Sys] Chip ID   : XXXXXXXX
[Sys] SDK       : 3.0.x
[Sys] Flash     : 4096 KB
[Sys] Free heap : 45000 B
[Sys] CPU freq  : 160 MHz
[Sys] Reset     : Power On
[Boot] Config loaded
[Boot] Brightness level 1 (raw=60) | Orientation 0
[Boot] Boot window 3000 ms — hold button for recovery
[WiFi] connecting... (AP fallback: "LED Clock")
[WiFi] connected to existing network
[NTP] timezone = HKT-8
[NTP] synced — 2025-01-01 12:00:00
[Weather] fetching city=Hong Kong...
[Weather] OK  code=800  25.0°C  clear sky
[Boot] complete — heap=38000 B
================================
[Heartbeat] 12:01:00 | heap=37500B | ntp=1 | wx=800 25.0C | orient=0 | idle=0
[Button] Click — brightness level 2 (raw=180)
[Button] Double-click — force NTP+weather refresh
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
| Naming | `update_NNN.zip` — sequential number or date (`update_003.zip`, `update_20260418.zip`) |
| Contents | Changed files only — **no `.github/`** (blocked by CI) |
| Paths | Relative to repo root (`src/main.cpp` not `/home/user/src/main.cpp`) |
| CHANGELOG | Update `zip_update/CHANGELOG.md` — first line controls build (`#!build` or `#!no-build`) |
| Cleanup | Zip is deleted automatically after successful application |

### Example: AI makes 3 file changes
```
zip_update/patches/update_003.zip
├── src/main.cpp            ← changed
├── gui/js/configuration.json ← changed
└── zip_update/CHANGELOG.md   ← updated release notes
```

The AI should **not** include `README.md`, `platformio.ini`, or any other
unchanged file in the zip.

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
    ├── main.cpp             ← application entry, state machine
    ├── StatusLED.*          ← onboard LED blink manager
    ├── TimeManager.*        ← NTP + manual time fallback
    ├── WeatherService.*     ← OpenWeatherMap HTTP fetch
    ├── DisplayManager.*     ← FastLED matrix driver
    ├── ClockFace.*          ← digit + scroll rendering
    ├── InfoColumns.*        ← date / weekday / weather pixel bars
    ├── PinDefinitions.h     ← hardware pin assignments (unchanged)
    ├── LEDMatrixLayout.h    ← snake-order mapping (unchanged)
    ├── FontData.*           ← 5×7 pixel font data (unchanged)
    └── ButtonHandler.*      ← debounced button state machine (unchanged)
```

### ⚠️ GitHub Actions workflow files cannot be self-updated

GitHub does **not** allow a workflow run to modify files inside `.github/workflows/`
(including `build.yml`). Any zip that contains a `.github/` path will be **rejected
automatically** by the CI safety check in `build.yml`:

```bash
if find _patch_tmp -type d -name '.github' | grep -q '.'; then
  echo "❌ Patch contains .github/ — aborting for security"
  exit 1
fi
```

**Rule: never include `.github/` or any `*.yml` workflow file in a patch zip.**

If the workflow file (`build.yml`) itself needs to be changed, the developer must
commit the new `build.yml` directly to the repository outside the patch mechanism.
