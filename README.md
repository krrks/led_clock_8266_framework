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

## Key Features

- **WiFi-safe DMA** LED output (NeoPixelBus I2S on GPIO3)
- **Recovery module** — self-contained bootloader with web UI (firmware OTA, file management, wireless serial monitor)
- **Modular architecture** — independent config, WiFi, NTP, web server, dashboard modules
- **No external framework dependency** — uses ESPAsyncWebServer, ArduinoJson, NeoPixelBus directly
- **Plain HTML/CSS/JS web UI** — no React/Node build step required
- **GitHub Actions CI/CD** — auto-builds firmware + LittleFS image on push/release
