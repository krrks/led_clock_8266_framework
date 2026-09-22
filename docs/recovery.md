# Recovery Mode

The **Recovery Manager** is a self-contained ESP8266 bootloader module designed
for reuse across projects. It lives in `src/recovery/` and has minimal
dependencies (ESPAsyncWebServer, ESPAsyncTCP, ArduinoJson, LittleFS).

## Triggers

Recovery mode is entered automatically at boot when:

1. **Button hold** — trigger pin (configurable GPIO) held during boot window (default 3s)
2. **Crash detection** — ESP reset reason is "Exception" or "Watchdog" (covers soft WDT too)
3. **Freeze watchdog** — Ticker ISR: if the main loop stops updating its heartbeat for 60 s (logical deadlock), the ISR locks interrupts and lets the hardware watchdog reset the chip ("Hardware Watchdog" reset reason → recovery). No RTC/SPI access from the ISR (safe during flash operations). Paused automatically during firmware uploads (5 min auto-resume). Covers normal mode AND recovery mode
4. **Heap watchdog** — free heap below 6 KB in normal mode → proactive recovery
5. **Software trigger** — `RecoveryManager::get().trigger()` called (BTN1 held 8 s in normal mode)

> Weather fetch failures do **not** trigger recovery — they are only logged
> and retried on the hourly schedule.

## Recovery Behaviour

1. Attempts to connect to saved WiFi (if credentials provided)
2. On WiFi failure: starts AP with configurable SSID (default: `RECOVERY`)
3. Starts recovery web server on port **80**
4. **Auto-reboot after 2 h** — if nobody exits recovery, the device reboots to normal mode by itself. Disabled after 3 consecutive crashes (no normal boot in between) so a broken app stays in recovery for OTA repair instead of crash-cycling
5. Serves a single-page recovery UI with three tabs:

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
| Boot window | ON (solid) |
| Normal mode | OFF |
| Recovery mode | Two short blinks (150 ms), then ~5 s rest |
