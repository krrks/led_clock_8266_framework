# Recovery Mode

The **Recovery Manager** is a self-contained ESP8266 bootloader module designed
for reuse across projects. It lives in `src/recovery/` and has minimal
dependencies (ESPAsyncWebServer, ESPAsyncTCP, ArduinoJson, LittleFS).

## Triggers

Recovery mode is entered automatically at boot when:

1. **Button hold** — trigger pin (configurable GPIO) held during boot window (default 3s)
2. **Crash detection** — ESP reset reason is "Exception" or "Watchdog"
3. **Software trigger** — `RecoveryManager::get().trigger()` called (also triggered by 5 consecutive weather fetch failures)

## Recovery Behaviour

1. Attempts to connect to saved WiFi (if credentials provided)
2. On WiFi failure: starts AP with configurable SSID (default: `RECOVERY`)
3. Starts recovery web server on port **8080**
4. Serves a single-page recovery UI with three tabs:

### Firmware
- Upload compiled `.bin` file
- OTA update with progress bar
- Auto-reboot after successful update

### Files
- List all files on LittleFS with sizes
- Delete individual files
- Useful for removing corrupted config or stale data

### Serial Monitor
- Real-time serial output streamed via WebSocket
- Send text input to device
- Auto-scroll with toggle

## Exiting Recovery

Click **"Reboot to Normal"** in the web UI, or access `/api/exit`.

## Using in Other Projects

Copy `src/recovery/` into any ESP8266 Arduino/PlatformIO project:

```cpp
#include "recovery/RecoveryManager.h"

void setup() {
    auto& rm = RecoveryManager::get();
    rm.triggerPin   = 5;           // GPIO for button trigger (0 = disabled)
    rm.bootWindowMs = 3000;        // How long to watch button
    rm.apSSID       = "MYDEVICE";  // AP mode SSID
    rm.apPassword   = "";          // Empty = open network
    rm.webPort      = 8080;        // Recovery web server port
    rm.staSSID      = savedSSID;   // Try to connect to this WiFi first
    rm.staPassword  = savedPass;
    rm.begin();

    if (rm.isActive()) return;  // Recovery mode active — skip normal init
    // ... normal setup ...
}

void loop() {
    rm.loop();
    if (rm.isActive()) return;  // Skip normal loop in recovery
    // ... normal loop ...
}
```

The recovery module embeds its HTML/JS/CSS as PROGMEM strings, so it works
even if LittleFS is corrupted. No external files needed.

## LED Indicator

| State | Pattern |
|-------|---------|
| Boot window | Fast blink (100ms) |
| Normal mode | OFF |
| Recovery mode | Slow blink (1s) |
