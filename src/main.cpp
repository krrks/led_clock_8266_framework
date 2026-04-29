// main.cpp — LED Matrix Clock v1.007
// Wemos D1 Mini (ESP8266) + 32×8 WS2812B + 4 buttons + onboard LED
//
// Button layout:
//   BTN1 MODE    GPIO5  D1  — cycle display mode / enter settings (3s)
//   BTN2 UP      GPIO14 D5  — brightness up / value +1 in settings
//   BTN3 DOWN    GPIO12 D6  — brightness down / value -1 in settings
//   BTN4 CONFIRM GPIO13 D7  — save & exit settings (click) / cancel (3s hold)
//
// Module layout:
//   AppState.h          — enums, constants, extern globals
//   BtnSimple.h         — header-only 3-level button (click / 3 s / 8 s)
//   ClockDisplay.h/.cpp — display primitives + face renderers
//   SettingsMode.h/.cpp — on-device settings editor
//   WeatherFetch.h/.cpp — OWM HTTP fetch + recovery trigger
//   main.cpp            — global variable definitions, setup(), loop()

#include <Arduino.h>
#include "LittleFS.h"
#define FASTLED_ALLOW_INTERRUPTS 0     // Most important for stability on ESP8266
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <time.h>

// Framework
#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "timeSync.h"
#include "dashboard.h"

// Project
#include "PinDefinitions.h"
#include "LEDMatrixLayout.h"
#include "AppState.h"
#include "BtnSimple.h"
#include "ClockDisplay.h"
#include "SettingsMode.h"
#include "WeatherFetch.h"

// ─────────────────────────────────────────────────────────────────────
// Global variable definitions  (declared extern in AppState.h)
// ─────────────────────────────────────────────────────────────────────
CRGB     leds[NUM_LEDS];
uint32_t displayMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];

AppMode  appMode  = AM_NORMAL;
DispMode dispMode = DM_CLOCK;

bool    ntpSynced      = false;
bool    weatherEnabled = true;
bool    wifiActive     = false;

int16_t weatherCode    = 0;
float   weatherTemp    = 0.0f;
char    weatherDesc[32]= "N/A";
int     weatherFails   = 0;

uint8_t  curRotation   = 0;   // 0=0°  1=90°CW  2=180°  3=270°CW
uint8_t  curFlip       = 0;   // 0=none  1=H-flip  2=V-flip

struct tm manualTm      = {};
uint32_t  manualBase    = 0;

bool pendingRedraw = false;
int  scrollOff     = 0;

int           settingsCursor = 0;
int           settingsActive[20];
int           settingsCount  = 0;
unsigned long tSettingsEntry = 0;

unsigned long tLastDisplay  = 0;
unsigned long tLastNtp      = 0;
unsigned long tLastWeather  = 0;
unsigned long tLastDash     = 0;
unsigned long tLastActivity = 0;
unsigned long tLastHeart    = 0;
unsigned long tShowIPUntil  = 0;
unsigned long tLedBlink     = 0;
bool          ledBlinkState = false;

// ─────────────────────────────────────────────────────────────────────
// Module-local
// ─────────────────────────────────────────────────────────────────────
static Btn btnMode, btnUp, btnDown, btnConfirm;

// ── Brightness ────────────────────────────────────────────────────────
static uint8_t mapBrightness(uint8_t idx) {
    if (idx == 0) return configManager.data.brightDim;
    if (idx == 2) return configManager.data.brightBrt;
    return configManager.data.brightMed;
}

void applyBrightness() {   // definition — declared extern in AppState.h
    FastLED.setBrightness(mapBrightness(configManager.data.brightness));
}

// ── Scroll frame interval (runtime) ──────────────────────────────────
static inline unsigned long scrollFrameMs() {
    return (unsigned long)configManager.data.scrollSpeed;
}

// ── Status LED (GPIO2, active LOW) ───────────────────────────────────
static void updateStatusLED() {
    if (appMode == AM_RECOVERY) {
        unsigned long now = millis();
        if (now - tLedBlink >= 1000UL) {
            tLedBlink = now; ledBlinkState = !ledBlinkState;
            digitalWrite(STATUS_LED_PIN, ledBlinkState ? LOW : HIGH);
        }
    } else {
        digitalWrite(STATUS_LED_PIN, HIGH);   // OFF (active-low)
    }
}

// ── Heartbeat ─────────────────────────────────────────────────────────
static const char* dmName(DispMode m) {
    switch (m) {
        case DM_DATE: return "date"; case DM_TEMP: return "temp";
        case DM_IP:   return "ip";   default:       return "clock";
    }
}

static void printHeartbeat() {
    struct tm t = {}; char tb[12] = "--:--:--"; bool ok = false;
    if (ntpSynced) {
        time_t n = time(nullptr);
        if (struct tm* p = localtime(&n)) { t = *p; ok = true; }
    }
    if (!ok && manualBase > 0) {
        time_t ts = mktime(&manualTm) + (time_t)((millis()-manualBase)/1000UL);
        if (struct tm* p = localtime(&ts)) { t = *p; ok = true; }
    }
    if (ok) snprintf(tb, sizeof(tb), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);

    String wifi = wifiActive
        ? (WiFiManager.isCaptivePortal() ? "AP:192.168.4.1"
           : (WiFi.status()==WL_CONNECTED ? WiFi.localIP().toString() : "offline"))
        : "off";

    Serial.printf("[Heart] %s heap=%u ntp=%d wx=%d %.1fC [%s] "
                  "app=%s disp=%s bri=%d rot=%d flip=%d spd=%d wifi=%s\n",
                  tb, ESP.getFreeHeap(), ntpSynced,
                  weatherCode, weatherTemp, weatherEnabled ? "on":"off",
                  appMode==AM_SETTINGS?"SET":(appMode==AM_RECOVERY?"REC":"NRM"),
                  dmName(dispMode), configManager.data.brightness,
                  curRotation, curFlip, configManager.data.scrollSpeed,
                  wifi.c_str());
}

// ── Dashboard push ────────────────────────────────────────────────────
static void updateDashboard() {
    struct tm t = {}; bool ok = false;
    if (ntpSynced) {
        time_t n = time(nullptr);
        if (struct tm* p = localtime(&n)) { t = *p; ok = true; }
    }
    if (!ok && manualBase > 0) {
        time_t ts = mktime(&manualTm) + (time_t)((millis()-manualBase)/1000UL);
        if (struct tm* p = localtime(&ts)) { t = *p; ok = true; }
    }
    if (!ok) return;

    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    strlcpy(dash.data.currentTime, tmp, sizeof(dash.data.currentTime));
    snprintf(tmp, sizeof(tmp), "%04d-%02d-%02d", t.tm_year+1900, t.tm_mon+1, t.tm_mday);
    strlcpy(dash.data.currentDate, tmp, sizeof(dash.data.currentDate));
    static const char* WDN[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    strlcpy(dash.data.weekday, WDN[t.tm_wday], sizeof(dash.data.weekday));
    dash.data.ntpSynced   = ntpSynced;
    dash.data.temperature = weatherTemp;
    dash.data.weatherCode = (uint16_t)weatherCode;
    strlcpy(dash.data.weatherDesc, weatherDesc, sizeof(dash.data.weatherDesc));
    dash.data.freeHeap   = (uint32_t)ESP.getFreeHeap();
    dash.data.uptime     = (uint32_t)(millis()/1000UL);
    dash.data.inRecovery = (appMode == AM_RECOVERY);
}

// ─────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println(F("\n================================"));
    Serial.println(F("  LED Matrix Clock  v1.007"));
    Serial.println(F("================================"));
    Serial.printf("[Sys] Reset:%s  Heap:%uB  CPU:%uMHz\n",
                  ESP.getResetReason().c_str(), ESP.getFreeHeap(), ESP.getCpuFreqMHz());

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);   // ON during boot

    btnMode.begin(BUTTON_1_PIN);
    btnUp.begin  (BUTTON_2_PIN);
    btnDown.begin(BUTTON_3_PIN);
    btnConfirm.begin(BUTTON_4_PIN);
    Serial.printf("[Boot] BTN MODE=GPIO%d UP=GPIO%d DOWN=GPIO%d CONFIRM=GPIO%d\n",
                  BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN, BUTTON_4_PIN);

    LittleFS.begin();
    configManager.begin();

    FastLED.addLeds<WS2812B, LED_MATRIX_PIN, GRB>(leds, NUM_LEDS);
    applyBrightness();
    FastLED.clear(true);
    curRotation    = configManager.data.rotation;
    curFlip        = configManager.data.flip;
    weatherEnabled = configManager.data.defaultWeather;
    Serial.printf("[Boot] bright=%d(raw=%d) rot=%d flip=%d spd=%d wx=%d wifi=%d\n",
                  configManager.data.brightness,
                  mapBrightness(configManager.data.brightness),
                  curRotation, curFlip, configManager.data.scrollSpeed,
                  weatherEnabled, configManager.data.wifiEnabled);

    // Web config save → immediate live update
    configManager.setConfigSaveCallback([]() {
        applyBrightness();
        curRotation    = configManager.data.rotation;
        curFlip        = configManager.data.flip;
        weatherEnabled = configManager.data.defaultWeather;
        if (!ntpSynced) {
            memset(&manualTm, 0, sizeof(manualTm));
            manualTm.tm_hour  = configManager.data.manualHour;
            manualTm.tm_min   = configManager.data.manualMinute;
            manualTm.tm_mday  = configManager.data.manualDay;
            manualTm.tm_mon   = configManager.data.manualMonth - 1;
            manualTm.tm_year  = configManager.data.manualYear - 1900;
            manualTm.tm_isdst = -1;
            mktime(&manualTm);
            manualBase = millis();
        }
        pendingRedraw = true;
        tLastActivity = millis();
        Serial.printf("[Config] bright=%d(raw=%d) rot=%d flip=%d spd=%d wx=%d\n",
                      configManager.data.brightness,
                      mapBrightness(configManager.data.brightness),
                      curRotation, curFlip, configManager.data.scrollSpeed,
                      weatherEnabled);
    });

    // ── Recovery detection ──────────────────────────────────────────
    String rsn = ESP.getResetReason();
    bool crashReset = (rsn == "Exception" || rsn.indexOf("Watchdog") >= 0);

    RTCData rtc = {}; bool rtcFlag = false;
    if (ESP.rtcUserMemoryRead(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc))) {
        if (rtc.magic == RTC_MAGIC && rtc.enterRecovery == 1) {
            rtcFlag = true;
            rtc.enterRecovery = 0;
            ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
        }
    }

    // ── Boot window: fast-blink, watch BTN1 ─────────────────────────
    bool bootHold = false;
    Serial.printf("[Boot] window %lums — hold BTN1(MODE) for recovery\n", BOOT_WINDOW_MS);
    unsigned long bootStart = millis();
    while (millis() - bootStart < BOOT_WINDOW_MS) {
        bool c, l, vl; btnMode.poll(c, l, vl);
        if (l || vl) { bootHold = true; break; }
        unsigned long n = millis();
        if (n - tLedBlink >= 100) {
            tLedBlink = n; ledBlinkState = !ledBlinkState;
            digitalWrite(STATUS_LED_PIN, ledBlinkState ? LOW : HIGH);
        }
        delay(10); yield();
    }
    if (crashReset || rtcFlag || bootHold) appMode = AM_RECOVERY;
    Serial.printf("[Boot] crash=%d rtc=%d btn=%d → recovery=%d\n",
                  crashReset, rtcFlag, bootHold, appMode==AM_RECOVERY);

    GUI.begin();
    dash.begin(DASH_INT_MS);

    // ── WiFi ────────────────────────────────────────────────────────
    if (appMode == AM_RECOVERY || configManager.data.wifiEnabled) {
        WiFiManager.begin(appMode==AM_RECOVERY ? "LED-CLOCK"
                                               : configManager.data.projectName, 15000);
        wifiActive = true;
        bool wifiOK = (WiFi.status()==WL_CONNECTED && !WiFiManager.isCaptivePortal());
        if (wifiOK) Serial.printf("[WiFi] IP %s\n", WiFi.localIP().toString().c_str());

        if (appMode == AM_NORMAL) {
            const char* tz = strlen(configManager.data.timezone) > 0
                             ? configManager.data.timezone : "HKT-8";
            Serial.printf("[NTP] tz=%s\n", tz);
            timeSync.begin(tz);

            if (timeSync.waitForSyncResult(10000) == 0) {
                ntpSynced = true;
                time_t n = time(nullptr); char tb[32];
                strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", localtime(&n));
                Serial.printf("[NTP] synced %s\n", tb);
            } else {
                memset(&manualTm, 0, sizeof(manualTm));
                manualTm.tm_hour  = configManager.data.manualHour;
                manualTm.tm_min   = configManager.data.manualMinute;
                manualTm.tm_mday  = configManager.data.manualDay;
                manualTm.tm_mon   = configManager.data.manualMonth - 1;
                manualTm.tm_year  = configManager.data.manualYear - 1900;
                manualTm.tm_isdst = -1;
                mktime(&manualTm);
                manualBase = millis();
                Serial.printf("[NTP] timeout — manual %02d:%02d\n",
                              configManager.data.manualHour, configManager.data.manualMinute);
            }
            fetchWeather();
        }
        if (wifiOK && appMode == AM_NORMAL) {
            dispMode = DM_IP; tShowIPUntil = millis() + IP_SHOW_MS; scrollOff = 0;
        }
    } else {
        WiFi.mode(WIFI_OFF); WiFi.forceSleepBegin();
        wifiActive = false;
        Serial.println("[WiFi] disabled — radio off");
    }

    unsigned long now = millis();
    tLastActivity = tLastNtp = tLastWeather = tLastDash = tLastHeart = now;
    tLastDisplay  = 0;
    digitalWrite(STATUS_LED_PIN, HIGH);   // OFF in normal/recovery (blink handled in loop)
    Serial.printf("[Boot] done  heap=%uB\n================================\n\n",
                  ESP.getFreeHeap());
}

// ─────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────
void loop() {
    if (wifiActive) WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();

    unsigned long now = millis();

    bool mClk, mLng, mVLng;
    bool uClk, uLng, uVLng;
    bool dClk, dLng, dVLng;
    bool cClk, cLng, cVLng;   // BTN4 CONFIRM
    btnMode.poll   (mClk, mLng, mVLng);
    btnUp.poll     (uClk, uLng, uVLng);
    btnDown.poll   (dClk, dLng, dVLng);
    btnConfirm.poll(cClk, cLng, cVLng);

    if (mClk||mLng||mVLng||uClk||uLng||uVLng||
        dClk||dLng||dVLng||cClk||cLng||cVLng)  tLastActivity = now;
    if (wifiActive && GUI.ws.count() > 0)       tLastActivity = now;
    bool isIdle = (now - tLastActivity > IDLE_TIMEOUT_MS);

    // ── Auto-return from IP ───────────────────────────────────────────
    if (tShowIPUntil > 0 && now >= tShowIPUntil && dispMode == DM_IP) {
        tShowIPUntil = 0; dispMode = DM_CLOCK; scrollOff = 0;
        pendingRedraw = true; Serial.println("[IP] auto-return to clock");
    }

    // ════════════════════════════════════════════════════════════════
    // Button dispatch
    // ════════════════════════════════════════════════════════════════

    // ── NORMAL mode ───────────────────────────────────────────────────
    if (appMode == AM_NORMAL) {

        // BTN1 MODE
        if      (mVLng) {
            Serial.println("[Btn] MODE 8 s → recovery");
            triggerRecovery();
        }
        else if (mLng) {
            Serial.println("[Btn] MODE 3 s → settings");
            appMode = AM_SETTINGS; buildActiveSettings();
            settingsCursor = 0; tSettingsEntry = now; scrollOff = 0;
            pendingRedraw = true;
        }
        else if (mClk) {
            // Cycle: CLOCK → DATE → TEMP → IP → CLOCK
            dispMode = (DispMode)((dispMode + 1) % DM_COUNT);
            tShowIPUntil = 0; scrollOff = 0;
            pendingRedraw = true;   // immediate redraw for every mode
            Serial.printf("[Btn] MODE click → %s\n", dmName(dispMode));
        }

        // BTN2 UP
        if      (uVLng) { triggerRecovery(); }
        else if (uLng) {
            Serial.println("[Btn] UP 3 s → force NTP+wx");
            if (!ntpSynced && timeSync.waitForSyncResult(3000)==0) ntpSynced = true;
            tLastNtp = now; fetchWeather(); tLastWeather = now;
            pendingRedraw = true;
        }
        else if (uClk) {
            uint8_t b = (configManager.data.brightness + 1) % 3;
            configManager.data.brightness = b; configManager.save(); applyBrightness();
            pendingRedraw = true;
            Serial.printf("[Btn] UP → bright %d(raw=%d)\n", b, mapBrightness(b));
        }

        // BTN3 DOWN
        if      (dVLng) { triggerRecovery(); }
        else if (dLng) {
            weatherEnabled = !weatherEnabled;
            configManager.data.defaultWeather = weatherEnabled; configManager.save();
            pendingRedraw = true;
            Serial.printf("[Btn] DOWN 3 s → weather %s\n", weatherEnabled?"ON":"OFF");
        }
        else if (dClk) {
            uint8_t b = (configManager.data.brightness == 0) ? 2
                        : configManager.data.brightness - 1;
            configManager.data.brightness = b; configManager.save(); applyBrightness();
            pendingRedraw = true;
            Serial.printf("[Btn] DOWN → bright %d(raw=%d)\n", b, mapBrightness(b));
        }

        // BTN4 CONFIRM — in normal mode: show IP for 8 s (quick shortcut)
        if (cClk) {
            dispMode = DM_IP; scrollOff = 0;
            tShowIPUntil = now + IP_SHOW_MS;
            pendingRedraw = true;
            Serial.println("[Btn] CONFIRM → show IP");
        }
    }

    // ── SETTINGS mode ─────────────────────────────────────────────────
    else if (appMode == AM_SETTINGS) {
        if (now - tSettingsEntry >= SETTINGS_TIMEOUT) {
            Serial.println("[Settings] timeout → save");
            saveAndExitSettings();
        } else {
            // BTN1 MODE: cycle items  /  long-press: save & exit  / vlong: recovery
            if      (mVLng) { saveAndExitSettings(); triggerRecovery(); }
            else if (mLng)  { Serial.println("[Btn] MODE 3 s → save"); saveAndExitSettings(); }
            else if (mClk && settingsCount > 0) {
                settingsCursor = (settingsCursor + 1) % settingsCount;
                scrollOff = 0; pendingRedraw = true;
                char buf[24]; getSettingStr(settingsActive[settingsCursor], buf, sizeof(buf));
                Serial.printf("[Settings] %d/%d %s\n", settingsCursor+1, settingsCount, buf);
            }

            // BTN2 UP: +1 (click) / +5 (long)
            if      (uLng || uVLng) { if (settingsCount > 0) adjustSetting(settingsActive[settingsCursor], +5); }
            else if (uClk)          { if (settingsCount > 0) adjustSetting(settingsActive[settingsCursor], +1); }

            // BTN3 DOWN: -1 (click) / -5 (long)
            if      (dLng || dVLng) { if (settingsCount > 0) adjustSetting(settingsActive[settingsCursor], -5); }
            else if (dClk)          { if (settingsCount > 0) adjustSetting(settingsActive[settingsCursor], -1); }

            // BTN4 CONFIRM click → save & exit  /  hold 3 s → cancel
            if      (cVLng) { Serial.println("[Btn] CONFIRM 8 s → recovery"); saveAndExitSettings(); triggerRecovery(); }
            else if (cLng)  { Serial.println("[Btn] CONFIRM 3 s → cancel"); cancelAndExitSettings(); }
            else if (cClk)  { Serial.println("[Btn] CONFIRM click → save"); saveAndExitSettings(); }
        }
    }

    // ── RECOVERY mode ─────────────────────────────────────────────────
    else {
        // BTN1 long-press clears RTC flag and reboots (exit recovery)
        if (mLng || mVLng) {
            Serial.println("[Recovery] BTN1 3 s → clear flag & reboot");
            RTCData rtc = { RTC_MAGIC, 0 };
            ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
            delay(50); ESP.restart();
        }
        // BTN4 CONFIRM click → same (convenient exit)
        if (cClk) {
            Serial.println("[Recovery] CONFIRM → clear flag & reboot");
            RTCData rtc = { RTC_MAGIC, 0 };
            ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
            delay(50); ESP.restart();
        }
    }

    // ════════════════════════════════════════════════════════════════
    // Render
    // ════════════════════════════════════════════════════════════════
    {
        bool scrolling = (appMode == AM_RECOVERY)
                      || (appMode == AM_NORMAL  && dispMode == DM_IP)
                      || (appMode == AM_SETTINGS);
        unsigned long frameMs = scrolling ? scrollFrameMs()
                                          : (isIdle ? IDLE_REFRESH : ACTIVE_REFRESH);
        if (pendingRedraw || now - tLastDisplay >= frameMs) {
            pendingRedraw = false; tLastDisplay = now;
            clearDisplay();
            switch (appMode) {
                case AM_RECOVERY:
                    drawScroll("RECOVERY", C_ORANGE);
                    break;
                case AM_SETTINGS:
                    drawSettingsFace();
                    break;
                default:
                    switch (dispMode) {
                        case DM_CLOCK: drawClockFace(); break;
                        case DM_DATE:  drawDateFace();  break;
                        case DM_TEMP:  drawTempFace();  break;
                        case DM_IP:    drawIPFace();    break;
                    }
            }
            flushDisplay();
        }
    }

    // ════════════════════════════════════════════════════════════════
    // Background tasks (normal mode only)
    // ════════════════════════════════════════════════════════════════
    if (appMode == AM_NORMAL) {
        if (now - tLastDash >= (unsigned long)DASH_INT_MS) {
            tLastDash = now; updateDashboard();
        }
        if (wifiActive && now - tLastNtp >= NTP_INTERVAL_MS) {
            tLastNtp = now;
            if (timeSync.waitForSyncResult(5000) == 0) {
                ntpSynced = true; Serial.println("[NTP] re-sync OK");
            }
        }
        if (wifiActive && weatherEnabled && now - tLastWeather >= WEATHER_INT_MS) {
            tLastWeather = now; fetchWeather();
        }
    }
    if (now - tLastHeart >= HEARTBEAT_MS) { tLastHeart = now; printHeartbeat(); }

    updateStatusLED();

    // Idle: ~20 Hz saves CPU; active: ~200 Hz keeps buttons snappy.
    if (isIdle && (!wifiActive || GUI.ws.count() == 0) && appMode == AM_NORMAL)
        delay(50);
    else
        delay(10);
}
