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
| Press               | Action                        |
|---------------------|-------------------------------|
| Single click        | Cycle brightness (0 → 1 → 2 → 0) |
| Double click        | Force NTP + weather refresh   |
| Hold ≥ 1 s          | Enter recovery mode           |
| Hold ≥ 5 s          | Reboot                        |

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
   - **City** – e.g. `Amsterdam`
   - **OpenWeatherMap API Key** – free at [openweathermap.org](https://openweathermap.org/api)
   - **Timezone** – POSIX string, e.g. `CET-1CEST,M3.5.0,M10.5.0/3`
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

## Project structure
```
led-clock/
├── platformio.ini
├── .gitignore
├── gui/js/
│   ├── configuration.json   ← edit to add/remove config fields
│   └── dashboard.json       ← edit to add/remove dashboard fields
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
