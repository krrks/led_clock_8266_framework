#! internal metadata line — ignored by release notes

Patch 001 — Multi-fix update

- Fix: LED matrix left-right mirror corrected in LEDMatrixLayout.h
- Fix: Web UI page-switching (BrowserRouter → HashRouter)
- Feat: Configuration page now shows all clock settings (brightness, timezone, weather, manual time)
- Feat: Dashboard page now shows live time, date, NTP status, weather and system metrics
- Feat: Default brightness set to 10% (~25/255)
- Feat: Default timezone set to HKT-8 (Hong Kong, UTC+8)
- Feat: Adaptive display refresh — 1 s when active, 30 s when idle (30 s no interaction)
- Feat: Web-socket client presence resets idle timer
- Feat: Runtime long-press triggers soft-restart into recovery via RTC memory flag
- Chore: platformio env renamed nodemcuv2 → d1_mini to match CI workflow
- Chore: FastLED added to lib_deps
