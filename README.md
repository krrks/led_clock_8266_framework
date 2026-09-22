# ESP8266 Recovery Module

Standalone, reusable recovery/bootloader module extracted from
[krrks/led_clock_8266_framework](https://github.com/krrks/led_clock_8266_framework).
This branch has an **orphan history** — it can be pushed to a new repository as-is.

## Features

- **Crash detection** — enters recovery after an Exception / Watchdog reset
- **Software trigger** — RTC-memory flag (button hold in the host app)
- **Boot-window button trigger** (configurable GPIO, default 3 s)
- **STA + AP fallback** — connects saved WiFi, falls back to an open AP
- **Web UI** (single page, embedded as PROGMEM): firmware OTA upload,
  LittleFS file manager, wireless serial monitor
- **Crash logging** — once per crash episode, bounded 2 KB (`/crash.log`)
- **2 h auto-reboot** — exits recovery by itself; disabled after 3
  consecutive crashes (crash-loop guard)
- **Distinctive LED pattern hook** — host renders two short blinks + rest

## Usage

Copy `src/recovery/` into any ESP8266 Arduino/PlatformIO project.
Dependencies: ESPAsyncWebServer, ESPAsyncTCP, ArduinoJson, LittleFS.

```cpp
#include "recovery/RecoveryManager.h"

void setup() {
    auto& rm = RecoveryManager::get();
    rm.triggerPin   = 5;           // GPIO for button trigger (0 = disabled)
    rm.bootWindowMs = 3000;        // How long to watch button
    rm.apSSID       = "MYDEVICE";  // AP mode SSID
    rm.apPassword   = "";          // Empty = open network
    rm.webPort      = 80;          // Recovery web server port
    rm.staSSID      = savedSSID;   // Try to connect to this WiFi first
    rm.staPassword  = savedPass;
    rm.begin();

    if (rm.isActive()) return;  // Recovery mode active — skip normal init
    // ... normal setup ...
}

void loop() {
    // Deferred reboot: web handlers set the flag, the main loop restarts.
    if (RecoveryManager::get().takeRebootRequest()) {
        delay(50);
        ESP.restart();
    }
    rm.loop();
    if (rm.isActive()) return;  // Skip normal loop in recovery
    // ... normal loop ...
}
```

## Docs

See [docs/recovery.md](docs/recovery.md) for triggers, behaviour and the
LED indicator table.
