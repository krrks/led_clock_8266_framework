#! internal metadata — ignored by release notes

Patch 002 — Fix compile errors + IoT Framework as library

- Fix: platformio.ini now declares esp8266-iot-framework as lib_dep
- Fix: removed extra_scripts (framework handles preBuild itself)
- Fix: added CONFIG_PATH / DASHBOARD_PATH build flags so project JSON
       files are copied into the framework before code-gen runs
- Fix: RTCData struct uses two uint32_t members (sizeof=8, multiple-of-4
       required by ESP.rtcUserMemoryRead/Write API)
- Fix: all implicit narrowing casts made explicit; static functions
       properly scoped; CRGB fields assigned by member (r/g/b)
- Feat: FastLED added to lib_deps
