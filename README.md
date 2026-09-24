# ESP8266 LED Matrix Clock

32×8 WS2812B matrix clock on Wemos D1 Mini (ESP8266). Standalone device — no cloud/app
required. Features a modular recovery bootloader with wireless serial debugging.

## Quick Links

- [Hardware & Pin Mapping](docs/hardware.md)
- [Display Modes & Layout](docs/display.md)
- [Button Reference](docs/buttons.md)
- [Settings Reference](docs/settings.md)
- [Build & Flash](docs/build.md)
- [Recovery Mode](docs/recovery.md)
- [Troubleshooting](docs/troubleshooting.md)

## Key Features

- **WiFi-safe DMA** LED output (NeoPixelBus I2S on GPIO3)
- **Recovery module** — self-contained bootloader with web UI (firmware OTA, file management, wireless serial monitor)
- **Modular architecture** — independent config, WiFi, NTP, web server, dashboard modules
- **No external framework dependency** — uses ESPAsyncWebServer, ArduinoJson, NeoPixelBus directly
- **Plain HTML/CSS/JS web UI** — no React/Node build step required
- **GitHub Actions CI/CD** — auto-builds firmware + LittleFS image on push/release

## GPIO Map

| GPIO | D1 Mini | Use | Notes |
|------|---------|-----|-------|
| 5 / 14 / 12 / 13 | D1/D5/D6/D7 | BTN1–4 | INPUT_PULLUP, active LOW |
| 2  | D4 | Status LED | active LOW (onboard) |
| 3  | RX/D9 | Matrix data | I2S DMA; shared with TTL RX |
| 4  | D2 | **Free** | Safe for input/output |
| 16 | D0 | **Free** | Output only (no pull-up/interrupt) |
| 15 | D8 | **Free** | Keep LOW at boot |
| A0 | ADC | **Free** | Analog 0–1 V (battery monitor) |

Avoid GPIO0 (flash mode), GPIO1 (UART TX), GPIO6–11 (SPI flash, not broken out), and
don't add an external pull-up on GPIO12 (MTDI strap). Full details: [docs/hardware.md](docs/hardware.md).

## Recovery Module Roadmap

The recovery module ([src/recovery/](src/recovery/)) is designed to be
**framework-independent and reusable** in other ESP8266/ESP32 projects: it ships
its own web UI (OTA update, file management, wireless serial monitor) and has no
dependency on clock-specific code.

The module is published as a standalone copy on the **orphan branch
`recovery-module`** (no shared history — ready to be exported into its own
repository). The clock firmware keeps the module integrated in `src/recovery/`.
