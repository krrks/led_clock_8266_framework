// main.cpp — LED Matrix Clock
// Wemos D1 Mini (ESP8266) + 32×8 WS2812B matrix
//
// Changes in this version:
//   • Default brightness 10% (25/255)
//   • Default timezone HKT-8 (Hong Kong, UTC+8)
//   • Adaptive refresh: 1 s active / 30 s idle
//   • Web-socket activity resets idle timer
//   • Runtime long-press → RTC flag → soft-restart into recovery
//   • Recovery detection: crash/watchdog reset or RTC flag

#include <Arduino.h>
#include "LittleFS.h"
#include <FastLED.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <time.h>

#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "timeSync.h"
#include "dashboard.h"

#include "PinDefinitions.h"
#include "LEDMatrixLayout.h"
#include "FontData.h"
#include "ButtonHandler.h"

// ─────────────────────────────────────────────────────
// Compile-time constants
// ─────────────────────────────────────────────────────
#define DEFAULT_BRIGHTNESS  25          // ~10 % of 255
#define BOOT_WINDOW_MS      3000UL      // window for long-press at boot
#define LONG_PRESS_MS       1000UL      // 1 s = recovery trigger
#define IDLE_TIMEOUT_MS     30000UL     // 30 s no activity → idle
#define ACTIVE_REFRESH_MS   1000UL      // 1 s display update when active
#define IDLE_REFRESH_MS     30000UL     // 30 s display update when idle
#define SCROLL_FRAME_MS     80UL        // ~12 fps for recovery scroll
#define NTP_INTERVAL_MS     3600000UL   // re-sync every 1 h
#define WEATHER_INTERVAL_MS 3600000UL   // re-fetch every 1 h
#define DASH_INTERVAL_MS    5000UL      // dashboard WebSocket cadence
#define LED_BLINK_BOOT      100UL       // fast blink: boot window
#define LED_BLINK_RECOVERY  1000UL      // slow blink: recovery mode

// RTC memory: survives soft-reset, cleared on power-on
#define RTC_MAGIC 0xC10CFA11UL
struct RTCData { uint32_t magic; bool enterRecovery; };

// ─────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────
CRGB        leds[NUM_LEDS];
uint32_t    displayMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];
ButtonHandler button(BUTTON_PIN);

bool inRecovery      = false;
bool ntpSynced       = false;
bool longPressHandled = false;

// Weather
int   weatherCode = 800;
float weatherTemp = 25.0f;
char  weatherDesc[32] = "Clear";

// Manual time fallback
struct tm manualTm   = {};
uint32_t  manualBase = 0;   // millis() at which manual time was set

// Timing
unsigned long lastDisplayRefresh = 0;
unsigned long lastNtpSync        = 0;
unsigned long lastWeatherSync    = 0;
unsigned long lastDashUpdate     = 0;
unsigned long lastUserActivity   = 0;
unsigned long lastLedBlink       = 0;
bool          ledBlinkState      = false;

// ─────────────────────────────────────────────────────
// Colour helpers
// ─────────────────────────────────────────────────────
static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

const uint32_t C_WHITE  = rgb(255, 255, 255);
const uint32_t C_ORANGE = rgb(255, 165,   0);
const uint32_t C_CYAN   = rgb(  0, 255, 255);

uint32_t weatherColor(int code) {
    if (code >= 200 && code < 300) return rgb(128,   0, 128); // thunderstorm → purple
    if (code >= 300 && code < 400) return rgb(  0, 128, 255); // drizzle      → sky blue
    if (code >= 500 && code < 600) return rgb(  0,   0, 255); // rain         → blue
    if (code >= 600 && code < 700) return rgb(200, 200, 255); // snow         → pale blue
    if (code >= 700 && code < 800) return rgb(128, 128, 128); // mist/fog     → grey
    if (code == 800)               return rgb(255, 220,   0); // clear sky    → gold
    if (code  > 800)               return rgb(180, 180, 180); // clouds       → light grey
    return C_WHITE;
}

// ─────────────────────────────────────────────────────
// Display helpers
// ─────────────────────────────────────────────────────
void clearDisplay() {
    memset(displayMatrix, 0, sizeof(displayMatrix));
}

// Draw one character at column x; returns columns consumed (without inter-char gap)
int drawChar(char c, int x, uint32_t color) {
    const uint8_t* fd = getCharFontData(c);
    uint8_t w = getCharWidth(c);
    if (!fd) return w;
    for (uint8_t col = 0; col < w; col++) {
        int px = x + (int)col;
        if (px < 0 || px >= MATRIX_WIDTH) continue;
        uint8_t bits = fd[col];
        for (uint8_t row = 0; row < DIGIT_HEIGHT; row++) {
            if ((bits >> row) & 1)
                displayMatrix[row][px] = color;
        }
    }
    return w;
}

// Scrolling text (recovery mode)
static int           scrollOff  = 0;
static unsigned long lastScroll = 0;

void drawScrollText(const char* txt, uint32_t color) {
    clearDisplay();

    // Total pixel-width of the string (chars + 1-px gaps)
    int totalW = 0;
    for (int i = 0; txt[i]; i++) totalW += getCharWidth(txt[i]) + 1;

    int x = (int)MATRIX_WIDTH - scrollOff;
    for (int i = 0; txt[i]; i++) {
        x += drawChar(txt[i], x, color);
        x++;
    }

    if (millis() - lastScroll >= SCROLL_FRAME_MS) {
        lastScroll = millis();
        if (++scrollOff >= totalW + (int)MATRIX_WIDTH)
            scrollOff = 0;
    }
}

// HH:MM centred in 32 columns
// Layout (5+1+5+1+1+1+5+1+5 = 25 px) with 3-4 px margins each side
void drawTime(int hour, int minute, uint32_t color) {
    int x = 3;                                              // left margin
    x += drawChar('0' + hour   / 10, x, color); x++;       // H-tens + gap
    x += drawChar('0' + hour   % 10, x, color); x++;       // H-units + gap
    x += drawChar(':',               x, color); x++;       // colon + gap
    x += drawChar('0' + minute / 10, x, color); x++;       // M-tens + gap
         drawChar('0' + minute % 10, x, color);            // M-units
}

// Bottom info bars (row 7)
//   cols  0-4  : week-of-month (white)
//   cols  6-12 : weekday 1-7 (cyan weekdays / orange weekends)
//   cols 14-17 : weather (colour-coded)
void drawInfoBars(int mday, int wday) {
    int weekOfMonth = (mday - 1) / 7 + 1;                  // 1-5
    for (int i = 0; i < weekOfMonth && i < 5; i++)
        displayMatrix[MATRIX_HEIGHT-1][i] = C_WHITE;

    uint32_t wc = (wday >= 6) ? C_ORANGE : C_CYAN;
    for (int i = 0; i < wday && i < 7; i++)
        displayMatrix[MATRIX_HEIGHT-1][6 + i] = wc;

    uint32_t wx = weatherColor(weatherCode);
    for (int i = 0; i < 4; i++)
        displayMatrix[MATRIX_HEIGHT-1][14 + i] = wx;
}

// Flush displayMatrix → FastLED buffer → hardware
void flushDisplay() {
    uint32_t buf[NUM_LEDS];
    convertToSnakeOrder(displayMatrix, buf);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB((buf[i] >> 16) & 0xFF,
                       (buf[i] >>  8) & 0xFF,
                        buf[i]        & 0xFF);
    }
    FastLED.show();
}

// ─────────────────────────────────────────────────────
// Time helpers
// ─────────────────────────────────────────────────────
struct tm* getTime() {
    if (ntpSynced) {
        time_t now = time(nullptr);
        return localtime(&now);
    }
    // Manual time + millis-drift
    time_t t = mktime(&manualTm) + (long)((millis() - manualBase) / 1000UL);
    return localtime(&t);
}

// ─────────────────────────────────────────────────────
// Weather (plain HTTP, OpenWeatherMap free tier)
// ─────────────────────────────────────────────────────
void fetchWeather() {
    if (WiFiManager.isCaptivePortal()) return;
    if (strlen(configManager.data.weatherApiKey) < 8) return;

    WiFiClient client;
    HTTPClient http;

    String url = F("http://api.openweathermap.org/data/2.5/weather?q=");
    url += configManager.data.weatherCity;
    url += F("&appid=");
    url += configManager.data.weatherApiKey;
    url += F("&units=metric");

    if (http.begin(client, url)) {
        int rc = http.GET();
        if (rc == HTTP_CODE_OK) {
            StaticJsonDocument<1024> doc;
            if (!deserializeJson(doc, http.getStream())) {
                weatherCode = doc["weather"][0]["id"] | 800;
                weatherTemp = doc["main"]["temp"]     | 25.0f;
                strlcpy(weatherDesc,
                        doc["weather"][0]["description"] | "Unknown",
                        sizeof(weatherDesc));
                Serial.printf("Weather: code=%d %.1f°C %s\n",
                              weatherCode, weatherTemp, weatherDesc);
            }
        }
        http.end();
    }
}

// ─────────────────────────────────────────────────────
// Dashboard data update
// ─────────────────────────────────────────────────────
void updateDashboard() {
    struct tm* t = getTime();
    if (!t) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             t->tm_hour, t->tm_min, t->tm_sec);
    strlcpy(dash.data.currentTime, buf, sizeof(dash.data.currentTime));

    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday);
    strlcpy(dash.data.currentDate, buf, sizeof(dash.data.currentDate));

    static const char* wd[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    strlcpy(dash.data.weekday, wd[t->tm_wday], sizeof(dash.data.weekday));

    dash.data.ntpSynced   = ntpSynced;
    dash.data.temperature = weatherTemp;
    dash.data.weatherCode = (uint16_t)weatherCode;
    strlcpy(dash.data.weatherDesc, weatherDesc, sizeof(dash.data.weatherDesc));
    dash.data.freeHeap   = ESP.getFreeHeap();
    dash.data.uptime     = millis() / 1000UL;
    dash.data.inRecovery = inRecovery;
}

// ─────────────────────────────────────────────────────
// Status LED
// ─────────────────────────────────────────────────────
void blinkLed(unsigned long rateMs) {
    if (millis() - lastLedBlink >= rateMs) {
        lastLedBlink  = millis();
        ledBlinkState = !ledBlinkState;
        digitalWrite(STATUS_LED_PIN, ledBlinkState ? LOW : HIGH); // active-LOW
    }
}

void updateStatusLed() {
    if (inRecovery) {
        blinkLed(LED_BLINK_RECOVERY);
    } else {
        digitalWrite(STATUS_LED_PIN, HIGH); // OFF in normal mode
    }
}

// ─────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);

    // ── Hardware init ──
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW); // ON during boot

    button.begin();
    LittleFS.begin();
    configManager.begin();

    FastLED.addLeds<WS2812B, LED_MATRIX_PIN, GRB>(leds, NUM_LEDS);
    uint8_t brightness = configManager.data.brightness;
    FastLED.setBrightness(brightness > 0 ? brightness : DEFAULT_BRIGHTNESS);
    FastLED.clear(true);

    // Apply brightness immediately whenever config is saved from the web UI
    configManager.setConfigSaveCallback([]() {
        FastLED.setBrightness(configManager.data.brightness);
        lastUserActivity = millis();
    });

    // ── Recovery detection: crash / watchdog reset ──
    String reason = ESP.getResetReason();
    bool crashReset = (reason == "Exception" ||
                       reason.indexOf("Watchdog") >= 0);

    // ── Recovery detection: RTC flag from runtime long-press + restart ──
    RTCData rtc = {};
    bool rtcRecovery = false;
    if (ESP.rtcUserMemoryRead(0, (uint32_t*)&rtc, sizeof(rtc))) {
        if (rtc.magic == RTC_MAGIC && rtc.enterRecovery) {
            rtcRecovery = true;
            rtc.enterRecovery = false;
            ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc, sizeof(rtc));
        }
    }

    // ── Boot window: 3 s, watch for long press ──
    bool bootLongPress = false;
    unsigned long bootStart = millis();
    while (millis() - bootStart < BOOT_WINDOW_MS) {
        button.update();
        if (button.isPressed() && button.getPressDuration() >= LONG_PRESS_MS) {
            bootLongPress = true;
            break;
        }
        blinkLed(LED_BLINK_BOOT);
        delay(10);
        yield();
    }

    inRecovery = crashReset || rtcRecovery || bootLongPress;
    if (inRecovery) {
        Serial.printf("Recovery (crash=%d rtc=%d btn=%d reason=%s)\n",
                      crashReset, rtcRecovery, bootLongPress, reason.c_str());
    }

    // ── Framework init ──
    GUI.begin();
    dash.begin(DASH_INTERVAL_MS);

    const char* apName = inRecovery ? "LED-CLOCK"
                                    : configManager.data.projectName;
    WiFiManager.begin(apName, 15000);

    // ── NTP + timezone (normal mode only) ──
    if (!inRecovery) {
        const char* tz = (strlen(configManager.data.timezone) > 0)
                         ? configManager.data.timezone
                         : "HKT-8";
        timeSync.begin(tz);

        if (timeSync.waitForSyncResult(10000) == 0) {
            ntpSynced = true;
            Serial.println("NTP synced");
        } else {
            // Manual time fallback with millis drift
            memset(&manualTm, 0, sizeof(manualTm));
            manualTm.tm_hour  = configManager.data.manualHour;
            manualTm.tm_min   = configManager.data.manualMinute;
            manualTm.tm_mday  = configManager.data.manualDay;
            manualTm.tm_mon   = configManager.data.manualMonth - 1;
            manualTm.tm_year  = configManager.data.manualYear - 1900;
            manualTm.tm_isdst = -1;
            mktime(&manualTm);  // normalise
            manualBase = millis();
            Serial.println("NTP timeout — using manual time");
        }

        fetchWeather();
    }

    lastUserActivity   = millis();
    lastNtpSync        = millis();
    lastWeatherSync    = millis();
    lastDisplayRefresh = 0; // force immediate first draw

    digitalWrite(STATUS_LED_PIN, HIGH); // LED off after boot
    Serial.println("Setup complete");
}

// ─────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────
void loop() {
    // ── Framework services ──
    WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();

    unsigned long now = millis();

    // ── Button (polled — avoids ButtonHandler static-variable bug) ──
    button.update();

    if (button.isPressed()) {
        lastUserActivity = now;

        // Long press in normal mode → signal recovery via RTC + restart
        if (!inRecovery && !longPressHandled
            && button.getPressDuration() >= LONG_PRESS_MS)
        {
            longPressHandled = true;
            RTCData rtc = { RTC_MAGIC, true };
            ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtc, sizeof(rtc));
            delay(50);
            ESP.restart();
        }
    } else {
        longPressHandled = false;
    }

    // ── Web activity: any open WebSocket client = someone is watching ──
    if (GUI.ws.count() > 0) lastUserActivity = now;

    // ── Idle detection ──
    bool isIdle = (now - lastUserActivity > IDLE_TIMEOUT_MS);

    // ── Display refresh ──
    if (inRecovery) {
        // Non-blocking scroll at SCROLL_FRAME_MS rate
        drawScrollText("RECOVERY", C_ORANGE);
        flushDisplay();
    } else {
        unsigned long interval = isIdle ? IDLE_REFRESH_MS : ACTIVE_REFRESH_MS;
        if (now - lastDisplayRefresh >= interval) {
            lastDisplayRefresh = now;
            clearDisplay();
            struct tm* t = getTime();
            if (t) {
                drawTime(t->tm_hour, t->tm_min, C_WHITE);
                int wday = (t->tm_wday == 0) ? 7 : t->tm_wday; // 0=Sun → 7
                drawInfoBars(t->tm_mday, wday);
            }
            flushDisplay();
        }
    }

    // ── Dashboard data (separate from web send rate) ──
    if (now - lastDashUpdate >= DASH_INTERVAL_MS) {
        lastDashUpdate = now;
        updateDashboard();
    }

    // ── Periodic NTP re-sync ──
    if (!inRecovery && now - lastNtpSync >= NTP_INTERVAL_MS) {
        lastNtpSync = now;
        if (timeSync.waitForSyncResult(5000) == 0) ntpSynced = true;
    }

    // ── Periodic weather re-fetch ──
    if (!inRecovery && now - lastWeatherSync >= WEATHER_INTERVAL_MS) {
        lastWeatherSync = now;
        fetchWeather();
    }

    // ── Status LED ──
    updateStatusLed();

    yield(); // let ESP8266 background tasks run
}
