#!build
## v1.2.0 — Serial output, full button actions, display orientation

### New features
- **Serial monitor**: boot banner with firmware version, chip ID, SDK, flash size, heap, reset reason
- **Serial monitor**: periodic heartbeat every 60 s (time, heap, NTP status, weather code, orientation)
- **Serial monitor**: all button events, config saves, WiFi/NTP/weather transitions logged
- **Button single-click**: cycle brightness Dim → Medium → Bright → Dim (saves to EEPROM)
- **Button double-click**: force immediate NTP re-sync + weather fetch
- **Button 5 s hold**: reboot device
- **Display orientation**: new config dropdown — Normal / 90° CW / 180° / 270° CW / H-Flip / V-Flip
- **Brightness**: changed from 0-255 raw slider to 3-level select (Dim=10 / Medium=60 / Bright=180)
- **README**: serial output examples, orientation table, AI patch workflow rules
- **.gitattributes**: added `*.zip binary` to prevent git corrupting patch zips

### Breaking changes
- Brightness config field semantics changed (0-255 raw → 0/1/2 level index).
  EEPROM auto-resets to defaults on first boot after flashing — this is normal.

### Notes
- 90°/270° apply a scaled transform to fill the 32×8 non-square display
- `displayOrientation` is a new uint8_t in the config struct — triggers EEPROM reset
