// main.cpp  –  ESP8266 LED Matrix Clock
// Framework: maakbaas/esp8266-iot-framework
// Board    : Wemos D1 Mini (ESP8266)
// ─────────────────────────────────────────────────────────────────────────────
//
// Operating modes
// ───────────────
//  NORMAL MODE   WiFi connected  → NTP + weather + full clock face
//                WiFi failed     → AP started, manual time via web config,
//                                  retry WiFi every 1 hour
//
//  RECOVERY MODE Entered when:
//                  • button held ≥ 1 s (boot window OR any time during normal)
//                  • crash / watchdog reset detected at boot
//                Shows scrolling "RECOVERY" text, activates web GUI (OTA,
//                WiFi setup, config editor).  Exit by rebooting.
//
// Onboard LED (GPIO2, active LOW)
// ────────────────────────────────
//   Boot window / WiFi connecting  : fast blink (100 ms)
//   Normal mode                    : OFF always
//   Recovery mode                  : slow blink (1 s)
//   OTA in progress                : solid ON  (handled by framework)

#include <Arduino.h>
#include <LittleFS.h>

// ── Framework headers ───────────────────────────────────────────────────────
#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "timeSync.h"
#include "dashboard.h"

// ── Project headers ─────────────────────────────────────────────────────────
#include "PinDefinitions.h"
#include "ButtonHandler.h"
#include "StatusLED.h"
#include "TimeManager.h"
#include "WeatherService.h"
#include "DisplayManager.h"

// ─────────────────────────────────────────────────────────────────────────────
// Global instances
// ─────────────────────────────────────────────────────────────────────────────
static ButtonHandler    button(BUTTON_PIN);
static StatusLED        statusLED(STATUS_LED_PIN, /*activeLow=*/true);
static TimeManager      timeManager;
static WeatherService   weather;
static DisplayManager   display;

// ─────────────────────────────────────────────────────────────────────────────
// State flags
// ─────────────────────────────────────────────────────────────────────────────
static bool g_recovery     = false;
static bool g_wifiOK       = false;  // true once STA is connected
static bool g_ntpOK        = false;

// ─────────────────────────────────────────────────────────────────────────────
// Simple non-blocking task helper
// ─────────────────────────────────────────────────────────────────────────────
struct Task {
    unsigned long intervalMs;
    unsigned long prevMs;
    bool          enabled;

    // Returns true and resets timer if interval has elapsed (or prevMs == 0).
    bool due() {
        if (!enabled) return false;
        unsigned long now = millis();
        if (prevMs == 0 || (now - prevMs) >= intervalMs) {
            prevMs = now;
            return true;
        }
        return false;
    }
    void arm()   { enabled = true;  prevMs = 0; }
    void disarm(){ enabled = false; }
};

static Task taskDisplay   = {30000UL,      0, false}; // 30 s display refresh
static Task taskSync      = {3600000UL,    0, false}; // 60 min NTP + weather
static Task taskWifiRetry = {3600000UL,    0, false}; // 60 min WiFi retry

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
static void enterRecovery();
static void initNormal();
static void initRecovery();
static void doSync();
static void doDisplay();
static void checkWifiConnected();  // polls for spontaneous (re)connection

// ─────────────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n[BOOT] ESP8266 LED Clock starting");

    // Hardware init
    statusLED.begin();
    statusLED.setMode(StatusLED::FAST_BLINK);

    button.begin();         // sets INPUT_PULLUP on BUTTON_PIN
    display.begin();        // FastLED init; matrix blank

    // ── Crash detection ──────────────────────────────────────────────────────
    String reason = ESP.getResetReason();
    Serial.printf("[BOOT] Reset reason: %s\n", reason.c_str());
    if (reason.indexOf("Exception") >= 0 ||
        reason.indexOf("Wdt Reset") >= 0) {
        Serial.println("[BOOT] Crash detected → recovery mode");
        g_recovery = true;
    }

    // ── 3-second boot window (skip on crash) ─────────────────────────────────
    if (!g_recovery) {
        Serial.println("[BOOT] 3-second boot window …");
        unsigned long windowStart  = millis();
        unsigned long btnHoldStart = 0;
        bool          btnWasDown   = false;

        while (millis() - windowStart < 3000) {
            bool btnDown = (digitalRead(BUTTON_PIN) == LOW); // active LOW

            if (btnDown) {
                if (!btnWasDown) { btnHoldStart = millis(); btnWasDown = true; }
                if (millis() - btnHoldStart >= 1000) {
                    Serial.println("[BOOT] Long press → recovery mode");
                    g_recovery = true;
                    break;
                }
            } else {
                btnWasDown = false;
            }

            statusLED.update();
            delay(20);
        }
    }

    // ── Framework + filesystem init ──────────────────────────────────────────
    LittleFS.begin();
    configManager.begin();
    GUI.begin();
    dash.begin(500);

    // Apply stored brightness immediately
    display.setBrightness(configManager.data.brightness);

    // ── Branch to mode ───────────────────────────────────────────────────────
    if (g_recovery) {
        initRecovery();
    } else {
        initNormal();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // ── Framework services (both modes) ─────────────────────────────────────
    WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();
    statusLED.update();

    // ── Button handling ──────────────────────────────────────────────────────
    button.update();

    // Long-press detection using isPressed() + getPressDuration()
    // This avoids the static-variable bug in ButtonHandler::processStateMachine()
    // where longPressTriggered never resets between presses.
    {
        static bool longHandled = false;
        if (button.isPressed()) {
            if (!longHandled && button.getPressDuration() >= 1000) {
                longHandled = true;
                if (!g_recovery) {
                    Serial.println("[BTN] Long press → entering recovery");
                    enterRecovery();
                }
            }
        } else {
            longHandled = false;
        }
    }

    // Normal button events (only in normal mode)
    if (!g_recovery) {
        ButtonEvent ev = button.getEvent();

        if (ev == BE_CLICK) {
            // Cycle brightness 0→1→2→0
            uint8_t b = (configManager.data.brightness + 1) % 3;
            configManager.data.brightness = b;
            configManager.save();
            display.setBrightness(b);
            Serial.printf("[BTN] Brightness → %d\n", b);

        } else if (ev == BE_DOUBLE_CLICK) {
            // Force immediate NTP + weather sync
            Serial.println("[BTN] Double-click → force sync");
            taskSync.arm();

        } else if (ev == BE_VERY_LONG_PRESS) {
            Serial.println("[BTN] Very-long press → reboot");
            ESP.restart();
        }
    } else {
        // Recovery mode: very long press reboots
        if (button.getEvent() == BE_VERY_LONG_PRESS) {
            Serial.println("[BTN] Very-long press in recovery → reboot");
            ESP.restart();
        }
    }

    // ── Recovery mode tasks ──────────────────────────────────────────────────
    if (g_recovery) {
        display.scrollTick();
        dash.data.recoveryMode = 1;
        return; // skip all normal-mode tasks
    }

    // ── Normal mode: detect if WiFi has (re)connected spontaneously ──────────
    checkWifiConnected();

    // ── Manual time update: detect config changes in the browser ─────────────
    if (!g_ntpOK) {
        if (timeManager.manualTimeChanged(
                configManager.data.manualYear,
                configManager.data.manualMonth,
                configManager.data.manualDay,
                configManager.data.manualHour,
                configManager.data.manualMinute,
                configManager.data.manualWeekday)) {
            Serial.println("[TIME] Manual time config changed – applying");
            timeManager.setManualTime(
                configManager.data.manualYear,
                configManager.data.manualMonth,
                configManager.data.manualDay,
                configManager.data.manualHour,
                configManager.data.manualMinute,
                configManager.data.manualWeekday);
        }
    }

    // ── WiFi retry task (normal mode, no WiFi) ───────────────────────────────
    if (!g_wifiOK && taskWifiRetry.due()) {
        Serial.println("[WIFI] 1-hour retry …");
        WiFi.reconnect();
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
            WiFiManager.loop();
            updater.loop();
            statusLED.update();
            yield();
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("[WIFI] Retry succeeded");
            g_wifiOK = true;
            statusLED.setMode(StatusLED::OFF);
            timeSync.begin(configManager.data.timezone);
            taskSync.arm();
            taskWifiRetry.disarm();
        } else {
            Serial.println("[WIFI] Retry failed – will try again in 1 hour");
        }
    }

    // ── Display refresh task (30 s) ──────────────────────────────────────────
    if (taskDisplay.due()) {
        doDisplay();
    }

    // ── NTP + weather sync task (60 min, WiFi only) ──────────────────────────
    if (g_wifiOK && taskSync.due()) {
        doSync();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// initNormal()  –  called from setup() when not entering recovery
// ─────────────────────────────────────────────────────────────────────────────
static void initNormal() {
    Serial.println("[INIT] Normal mode – connecting WiFi …");
    statusLED.setMode(StatusLED::FAST_BLINK);

    // 15-second WiFi attempt; if it fails WiFiManager starts its captive AP
    WiFiManager.begin(configManager.data.projectName, 15000);

    g_wifiOK = (WiFiManager.SSID().length() > 0);

    if (g_wifiOK) {
        Serial.printf("[WIFI] Connected to: %s\n",
                      WiFiManager.SSID().c_str());
        statusLED.setMode(StatusLED::OFF);
        timeSync.begin(configManager.data.timezone);
        taskSync.arm();     // sync NTP + weather immediately
        taskWifiRetry.disarm();
    } else {
        Serial.println("[WIFI] Failed – starting AP, using manual time");
        statusLED.setMode(StatusLED::OFF); // LED stays OFF in normal mode

        // Seed manual time from last-saved config values
        timeManager.setManualTime(
            configManager.data.manualYear,
            configManager.data.manualMonth,
            configManager.data.manualDay,
            configManager.data.manualHour,
            configManager.data.manualMinute,
            configManager.data.manualWeekday);

        taskSync.disarm();
        taskWifiRetry.arm(); // retry WiFi every hour
    }

    weather.begin();
    taskDisplay.arm(); // render first frame immediately
}

// ─────────────────────────────────────────────────────────────────────────────
// initRecovery()  –  called from setup() on crash or boot-window press
// ─────────────────────────────────────────────────────────────────────────────
static void initRecovery() {
    Serial.println("[INIT] Recovery mode");
    g_recovery = true;
    statusLED.setMode(StatusLED::SLOW_BLINK);

    // Try stored WiFi with shorter timeout; fall through to AP on failure
    WiFiManager.begin(configManager.data.projectName, 10000);

    display.showRecovery();

    taskDisplay.disarm();
    taskSync.disarm();
    taskWifiRetry.disarm();
}

// ─────────────────────────────────────────────────────────────────────────────
// enterRecovery()  –  called at runtime (long press or error threshold)
// ─────────────────────────────────────────────────────────────────────────────
static void enterRecovery() {
    g_recovery = true;
    statusLED.setMode(StatusLED::SLOW_BLINK);
    display.showRecovery();

    taskDisplay.disarm();
    taskSync.disarm();
    taskWifiRetry.disarm();

    // If STA is already connected, the web GUI remains accessible on local IP.
    // If not, WiFiManager should already have an AP running; if not, start one.
    if (!g_wifiOK) {
        WiFiManager.begin(configManager.data.projectName, 5000);
    }

    Serial.println("[RECOVERY] Active – reboot to exit");
}

// ─────────────────────────────────────────────────────────────────────────────
// doSync()  –  NTP check + weather fetch (runs every 60 min when WiFi OK)
// ─────────────────────────────────────────────────────────────────────────────
static void doSync() {
    Serial.println("[SYNC] Starting");

    // ── NTP ──────────────────────────────────────────────────────────────────
    if (!timeSync.isSynced()) {
        Serial.println("[NTP] Waiting for sync …");
        timeSync.waitForSyncResult(10000);
    }
    g_ntpOK = timeSync.isSynced();
    Serial.printf("[NTP] %s\n", g_ntpOK ? "OK" : "not synced");

    // ── Weather ───────────────────────────────────────────────────────────────
    if (strlen(configManager.data.owmApiKey) == 0) {
        Serial.println("[WEATHER] No API key configured – skipping");
        return;
    }

    bool ok = weather.fetch(configManager.data.city, configManager.data.owmApiKey);
    if (ok) {
        weather.resetFailCount();
    } else {
        weather.incrementFailCount();
        Serial.printf("[WEATHER] Fail count: %d / %d\n",
                      weather.getFailCount(), WEATHER_FAIL_LIMIT);
        if (weather.getFailCount() >= WEATHER_FAIL_LIMIT) {
            Serial.println("[WEATHER] Too many failures → recovery");
            enterRecovery();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// doDisplay()  –  render the clock face (runs every 30 s)
// ─────────────────────────────────────────────────────────────────────────────
static void doDisplay() {
    int hour, minute, second;
    int year, month, day, weekday;

    timeManager.getTimeComponents(hour, minute, second);
    timeManager.getDateComponents(year, month, day, weekday);

    uint32_t wxColor = weather.getConditionColor();

    display.render(hour, minute, weekday, day, wxColor);

    // ── Update live dashboard ─────────────────────────────────────────────────
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    strncpy(dash.data.currentTime, buf, sizeof(dash.data.currentTime));

    dash.data.weatherCode  = weather.getConditionCode();
    dash.data.wifiRSSI     = g_wifiOK ? (int)WiFi.RSSI() : 0;
    dash.data.ntpSynced    = g_ntpOK ? 1 : 0;
    dash.data.recoveryMode = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// checkWifiConnected()  –  detect spontaneous STA connection
// (e.g. user configured new WiFi through captive portal)
// ─────────────────────────────────────────────────────────────────────────────
static void checkWifiConnected() {
    if (g_wifiOK) return;                         // already connected
    if (WiFi.status() != WL_CONNECTED) return;    // not yet

    Serial.println("[WIFI] Spontaneous connection detected");
    g_wifiOK = true;
    statusLED.setMode(StatusLED::OFF);
    timeSync.begin(configManager.data.timezone);
    taskSync.arm();
    taskWifiRetry.disarm();
}
