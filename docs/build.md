# Build & Flash

## Prerequisites

- [VS Code](https://code.visualstudio.com/) + [PlatformIO extension](https://platformio.org/install/ide?install=vscode)
- No Node.js/npm required (web UI is static HTML/CSS/JS)

## Build

```
PlatformIO: Build (Ctrl+Alt+B)
```

Or from command line:
```bash
pio run -e d1_mini
```

Builds `firmware.bin` to `.pio/build/d1_mini/firmware.bin`.

## Build LittleFS Image

```bash
pio run -e d1_mini --target buildfs
```

Builds `littlefs.bin` from `data/` directory. Flash once to upload web UI files.

## Flash

```
PlatformIO: Upload (Ctrl+Alt+U)              # firmware only
PlatformIO: Upload Filesystem Image           # web UI files
```

Serial monitor: `pio device monitor -b 115200`

## First Configuration

1. On first boot with no WiFi credentials, device starts AP **LED-CLOCK** (open).
2. Connect to this AP → open `http://192.168.4.1` in browser.
3. Go to **Settings** and set:
   - **WiFi SSID / Password**
   - **Weather API Key** — free from [openweathermap.org](https://openweathermap.org/api)
   - **Weather City** — e.g. `Hong Kong`
   - **Timezone** — POSIX string, e.g. `HKT-8`
4. Save → device reboots and connects to your WiFi.

## OTA Firmware Update

1. Build → `firmware.bin`
2. Web UI → Settings → Firmware Upload → select `firmware.bin`
3. Device reboots automatically after OTA.

## Project Structure

```
led_clock_8266_framework/
├── .github/workflows/build.yml  # CI/CD: build + release
├── platformio.ini               # PlatformIO config
├── data/                        # LittleFS web UI files
│   ├── index.html
│   ├── style.css
│   └── app.js
├── docs/                        # Documentation (modular)
├── src/
│   ├── main.cpp                 # Entry point
│   ├── AppState.h               # Shared enums/globals
│   ├── PinDefinitions.h         # GPIO assignments + pin table
│   ├── BtnSimple.h              # Button state machine
│   ├── NeoStrip.h               # NeoPixelBus DMA strip
│   ├── LEDMatrixLayout.h        # Column-snake mapping
│   ├── FontData.h/.cpp          # 5x7 pixel font
│   ├── ClockDisplay.h/.cpp      # Display primitives + faces
│   ├── SettingsMode.h/.cpp      # On-device settings editor
│   ├── WeatherFetch.h/.cpp      # OWM HTTP client
│   ├── config/                  # Config manager (LittleFS JSON)
│   ├── wifi/                    # WiFi service (STA+AP)
│   ├── ntp/                     # NTP client
│   ├── web/                     # HTTP server + dashboard
│   └── recovery/                # ★ Reusable recovery module
└── zip_update/                  # CI patch workflow
```

## CI/CD (GitHub Actions)

Push to any branch triggers `build.yml`:
1. **Pre-flight**: checks for patches
2. **Apply patch**: extracts zip patches
3. **Build**: compiles firmware + LittleFS image
4. **Release**: creates GitHub Release on `main` branch
5. **Cleanup**: removes old releases/runs
