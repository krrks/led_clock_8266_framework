# Settings Reference

## Configuration Access

1. **Web UI**: Open device IP in browser → Settings tab
2. **On-device**: Hold BTN1 3s → LED matrix shows settings in cyan

## Settings List

| Setting | Range | Default | Description |
|---------|-------|---------|-------------|
| Project Name | text | LED Clock | Device name |
| Brightness | 0-2 | 0 (dim) | Active brightness level |
| Bright Dim PWM | 1-200 | 1 | Dim level PWM value |
| Bright Med PWM | 1-255 | 128 | Medium level PWM value |
| Bright High PWM | 1-255 | 255 | Bright level PWM value |
| Use 24h | on/off | on | 24h or 12h format |
| Rotation | 0-3 | 0 | 0=0°, 1=90°CW, 2=180°, 3=270°CW |
| Flip | 0-2 | 0 | 0=none, 1=H-flip, 2=V-flip |
| Scroll Speed | 30-200 | 80 | ms per column |
| Timezone | POSIX string | HKT-8 | e.g. `PST8PDT,M3.2.0,M11.1.0` |
| WiFi Enabled | on/off | on | Enable WiFi radio |
| WiFi SSID | text | — | Network name |
| WiFi Password | text | — | Network password |
| Weather Enabled | on/off | on | Show weather bar |
| Weather API Key | text | — | OpenWeatherMap API key |
| Weather City | text | Hong Kong | City name |
| Manual Hour | 0-23 | 12 | Fallback time |
| Manual Minute | 0-59 | 0 | Fallback time |
| Manual Day | 1-31 | 1 | Fallback date |
| Manual Month | 1-12 | 1 | Fallback date |
| Manual Year | 2024-2099 | 2025 | Fallback date |
| Manual Weekday | Mon-Sun | Mon | Fallback date |
| Serial Monitor (Wired) | on/off | on | Physical serial output |
| Serial Monitor (Wireless) | on/off | off | Web serial via WebSocket |

## Timezone Presets (on-device editor)

18 POSIX timezone presets available in the on-device editor:
UTC, GMT, CET+1, EET+2, MSK+3, IST+5, PKT+5, HKT+8, CST+8, JST+9,
AEST, NZST, EST-5, CST-6, MST-7, PST-8, ART-3, BRT-3.

Web UI supports any valid POSIX timezone string.

## Pin Settings (Web UI only)

The **Pins** tab in the web UI displays all GPIO assignments with current states:
- GPIO number, name, description, and pin mode
- Read-only display for reference during hardware debugging
