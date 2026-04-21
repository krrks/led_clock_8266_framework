// main.cpp — LED Matrix Clock
// Wemos D1 Mini (ESP8266) + 32×8 WS2812B matrix
//
// Built against maakbaas/esp8266-iot-framework (lib_dep).
// Generated headers (config.h, dash.h) come from the framework's
// preBuild scripts driven by our gui/js/configuration.json and
// gui/js/dashboard.json.

#include <Arduino.h>
#include "LittleFS.h"
#include <FastLED.h>
#include <ArduinoJson.h>          // v6
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <time.h>

// Framework singletons
#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "timeSync.h"
#include "dashboard.h"

// Project headers
#include "PinDefinitions.h"
#include "LEDMatrixLayout.h"
#include "FontData.h"
#include "ButtonHandler.h"

// ─────────────────────────────────────────────────────────────────────
// Compile-time constants
// ─────────────────────────────────────────────────────────────────────
#define DEFAULT_BRIGHTNESS   25UL       // ~10 % of 255
#define BOOT_WINDOW_MS     3000UL       // silent boot window
#define LONG_PRESS_MS      1000UL       // triggers recovery
#define IDLE_TIMEOUT_MS   30000UL       // inactivity → idle mode
#define ACTIVE_REFRESH_MS  1000UL       // display update when active
#define IDLE_REFRESH_MS   30000UL       // display update when idle
#define SCROLL_FRAME_MS     80UL        // ~12 fps scroll
#define NTP_INTERVAL_MS  3600000UL      // NTP re-sync interval
#define WEATHER_INT_MS   3600000UL      // weather re-fetch interval
#define DASH_INT_MS        5000         // dashboard WebSocket cadence (int)
#define LED_BLINK_BOOT      100UL
#define LED_BLINK_REC      1000UL

// Consecutive *network* failures (not config errors) before recovery.
// 401 / 400 are config errors and do NOT count against this limit.
#define WEATHER_FAIL_LIMIT  5

// RTC memory: survives software reset, lost on power-cycle.
// Two uint32_t → sizeof == 8 (multiple of 4, required by API).
#define RTC_MAGIC 0xC10CFA11UL
struct RTCData {
    uint32_t magic;
    uint32_t enterRecovery; // 1 = enter recovery on next boot
};

// ─────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────
CRGB        leds[NUM_LEDS];
uint32_t    displayMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];
ButtonHandler button(BUTTON_PIN);

bool inRecovery       = false;
bool ntpSynced        = false;
bool longPressHandled = false;

// Weather state
int16_t  weatherCode       = 800;
float    weatherTemp       = 25.0f;
char     weatherDesc[32]   = "Clear";
int      weatherFailCount  = 0;   // consecutive *network* failures only

// Manual time fallback
struct tm manualTm   = {};
uint32_t  manualBase = 0;   // millis() snapshot when manual time was set

// Timing bookmarks
unsigned long lastDisplayRefresh = 0;
unsigned long lastNtpSync        = 0;
unsigned long lastWeatherSync    = 0;
unsigned long lastDashUpdate     = 0;
unsigned long lastUserActivity   = 0;
unsigned long lastLedBlink       = 0;
bool          ledBlinkState      = false;

// Scroll state (recovery text)
static int           scrollOff  = 0;
static unsigned long lastScroll = 0;

// ─────────────────────────────────────────────────────────────────────
// Colour helpers
// ─────────────────────────────────────────────────────────────────────
static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

const uint32_t C_WHITE  = rgb(255, 255, 255);
const uint32_t C_ORANGE = rgb(255, 165,   0);
const uint32_t C_CYAN   = rgb(  0, 255, 255);

static uint32_t weatherColor(int16_t code) {
    if (code >= 200 && code < 300) return rgb(128,   0, 128); // thunderstorm
    if (code >= 300 && code < 400) return rgb(  0, 128, 255); // drizzle
    if (code >= 500 && code < 600) return rgb(  0,   0, 255); // rain
    if (code >= 600 && code < 700) return rgb(200, 200, 255); // snow
    if (code >= 700 && code < 800) return rgb(128, 128, 128); // mist / fog
    if (code == 800)               return rgb(255, 220,   0); // clear sky
    if (code  > 800)               return rgb(180, 180, 180); // clouds
    return C_WHITE;
}

// ─────────────────────────────────────────────────────────────────────
// Display primitives
// ─────────────────────────────────────────────────────────────────────
static void clearDisplay() {
    memset(displayMatrix, 0, sizeof(displayMatrix));
}

// Draw one character at pixel column x; returns glyph width consumed.
static int drawChar(char c, int x, uint32_t color) {
    const uint8_t* fd = getCharFontData(c);
    uint8_t w = getCharWidth(c);
    if (!fd) return (int)w;
    for (uint8_t col = 0; col < w; col++) {
        int px = x + (int)col;
        if (px < 0 || px >= (int)MATRIX_WIDTH) continue;
        uint8_t bits = fd[col];
        for (uint8_t row = 0; row < DIGIT_HEIGHT; row++) {
            if ((bits >> row) & 1)
                displayMatrix[row][px] = color;
        }
    }
    return (int)w;
}

// Scrolling text for recovery mode.
static void drawScrollText(const char* txt, uint32_t color) {
    clearDisplay();

    int totalW = 0;
    for (int i = 0; txt[i]; i++) totalW += (int)getCharWidth(txt[i]) + 1;

    int x = (int)MATRIX_WIDTH - scrollOff;
    for (int i = 0; txt[i]; i++) {
        x += drawChar(txt[i], x, color) + 1;
    }

    if (millis() - lastScroll >= SCROLL_FRAME_MS) {
        lastScroll = millis();
        if (++scrollOff >= totalW + (int)MATRIX_WIDTH)
            scrollOff = 0;
    }
}

// Draw HH:MM centred in the 32-column display.
static void drawTime(int hour, int minute, uint32_t color) {
    int x = 3;
    x += drawChar((char)('0' + hour   / 10), x, color); x++;
    x += drawChar((char)('0' + hour   % 10), x, color); x++;
    x += drawChar(':',                        x, color); x++;
    x += drawChar((char)('0' + minute / 10), x, color); x++;
         drawChar((char)('0' + minute % 10), x, color);
}

// Draw the three single-row info bars at the bottom (row 7).
static void drawInfoBars(int mday, int wday) {
    int weekOfMonth = (mday - 1) / 7 + 1;
    for (int i = 0; i < weekOfMonth && i < 5; i++)
        displayMatrix[MATRIX_HEIGHT - 1][i] = C_WHITE;

    uint32_t wc = (wday >= 6) ? C_ORANGE : C_CYAN;
    for (int i = 0; i < wday && i < 7; i++)
        displayMatrix[MATRIX_HEIGHT - 1][6 + i] = wc;

    uint32_t wx = weatherColor(weatherCode);
    for (int i = 0; i < 4; i++)
        displayMatrix[MATRIX_HEIGHT - 1][14 + i] = wx;
}

// Flush displayMatrix → FastLED LEDs → hardware.
static void flushDisplay() {
    uint32_t buf[NUM_LEDS];
    convertToSnakeOrder(displayMatrix, buf);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].r = (uint8_t)((buf[i] >> 16) & 0xFF);
        leds[i].g = (uint8_t)((buf[i] >>  8) & 0xFF);
        leds[i].b = (uint8_t)( buf[i]        & 0xFF);
    }
    FastLED.show();
}

// ─────────────────────────────────────────────────────────────────────
// Time helper
// ─────────────────────────────────────────────────────────────────────
static struct tm* getTime() {
    if (ntpSynced) {
        time_t now = time(nullptr);
        return localtime(&now);
    }
    time_t t = mktime(&manualTm) + (long)((millis() - manualBase) / 1000UL);
    return localtime(&t);
}

// ─────────────────────────────────────────────────────────────────────
// Weather fetch (plain HTTP, OpenWeatherMap free tier)
//
// Error handling:
//   401/400  → API key wrong / not yet activated; log clearly but do
//              NOT increment weatherFailCount (config issue, not network).
//   Other 4xx/5xx, timeout → increment weatherFailCount; trigger
//              recovery after WEATHER_FAIL_LIMIT consecutive failures.
// ─────────────────────────────────────────────────────────────────────
static void fetchWeather() {
    if (WiFiManager.isCaptivePortal()) return;

    const char* city   = configManager.data.weatherCity;
    const char* apiKey = configManager.data.weatherApiKey;

    if (strlen(city) == 0 || strlen(apiKey) < 8) {
        Serial.println("[Weather] skip — city or API key not configured");
        return;
    }

    Serial.printf("[Weather] fetching city=%s...\n", city);

    WiFiClient client;
    HTTPClient http;

    String url = F("http://api.openweathermap.org/data/2.5/weather?q=");
    url += city;
    url += F("&appid=");
    url += apiKey;
    url += F("&units=metric");

    http.setTimeout(8000);
    if (!http.begin(client, url)) {
        Serial.println("[Weather] http.begin() failed");
        weatherFailCount++;
        return;
    }

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        StaticJsonDocument<1024> doc;
        DeserializationError err = deserializeJson(doc, http.getStream());
        if (!err) {
            weatherCode = (int16_t)(doc["weather"][0]["id"]   | 800);
            weatherTemp =           doc["main"]["temp"]        | 25.0f;
            strlcpy(weatherDesc,
                    doc["weather"][0]["description"] | "Unknown",
                    sizeof(weatherDesc));
            weatherFailCount = 0;   // reset on success
            Serial.printf("[Weather] OK  code=%d  %.1f°C  %s\n",
                          weatherCode, weatherTemp, weatherDesc);
        } else {
            Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
            weatherFailCount++;
        }

    } else if (httpCode == 401) {
        // Bad API key — this is a configuration problem, not a network
        // failure.  Log clearly and do NOT count toward recovery trigger.
        Serial.println("[Weather] ERROR 401 — API key invalid or not yet");
        Serial.println("          activated. Visit openweathermap.org,");
        Serial.println("          check your key, then update it in the");
        Serial.println("          web UI under Configuration > API Key.");

    } else if (httpCode == 400) {
        Serial.printf("[Weather] ERROR 400 — bad request (check city name: \"%s\")\n", city);
        // Also a config issue — don't count toward network fail limit.

    } else if (httpCode == 404) {
        Serial.printf("[Weather] ERROR 404 — city \"%s\" not found\n", city);
        // Config issue.

    } else {
        Serial.printf("[Weather] HTTP error: %d\n", httpCode);
        weatherFailCount++;
    }

    http.end();

    // Enter recovery after too many consecutive *network* failures.
    if (weatherFailCount >= WEATHER_FAIL_LIMIT) {
        Serial.printf("[Weather] %d consecutive network failures — entering recovery\n",
                      weatherFailCount);
        RTCData rtc = { RTC_MAGIC, 1 };
        ESP.rtcUserMemoryWrite(0,
            reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
        delay(50);
        ESP.restart();
    }
}

// ─────────────────────────────────────────────────────────────────────
// Dashboard data fill
// ─────────────────────────────────────────────────────────────────────
static void updateDashboard() {
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
    dash.data.freeHeap    = (uint32_t)ESP.getFreeHeap();
    dash.data.uptime      = (uint32_t)(millis() / 1000UL);
    dash.data.inRecovery  = inRecovery;
}

// ─────────────────────────────────────────────────────────────────────
// Status LED
// ─────────────────────────────────────────────────────────────────────
static void blinkLed(unsigned long rateMs) {
    if (millis() - lastLedBlink >= rateMs) {
        lastLedBlink  = millis();
        ledBlinkState = !ledBlinkState;
        digitalWrite(STATUS_LED_PIN, ledBlinkState ? LOW : HIGH);
    }
}

static void updateStatusLed() {
    if (inRecovery) {
        blinkLed(LED_BLINK_REC);
    } else {
        digitalWrite(STATUS_LED_PIN, HIGH); // OFF in normal mode
    }
}

// ─────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    button.begin();
    LittleFS.begin();
    configManager.begin();

    FastLED.addLeds<WS2812B, LED_MATRIX_PIN, GRB>(leds, NUM_LEDS);
    uint8_t brt = configManager.data.brightness;
    FastLED.setBrightness(brt > 0 ? brt : (uint8_t)DEFAULT_BRIGHTNESS);
    FastLED.clear(true);

    configManager.setConfigSaveCallback([]() {
        FastLED.setBrightness(configManager.data.brightness);
        lastUserActivity = millis();
    });

    // Recovery detection 1: crash / watchdog reset
    String reason     = ESP.getResetReason();
    bool   crashReset = (reason == "Exception" ||
                         reason.indexOf("Watchdog") >= 0);

    // Recovery detection 2: runtime long-press → RTC flag
    RTCData rtc = {};
    bool rtcRecovery = false;
    if (ESP.rtcUserMemoryRead(0,
            reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc)))
    {
        if (rtc.magic == RTC_MAGIC && rtc.enterRecovery == 1) {
            rtcRecovery       = true;
            rtc.enterRecovery = 0;
            ESP.rtcUserMemoryWrite(0,
                reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
        }
    }

    // Boot window: watch for button long-press
    bool bootLongPress = false;
    unsigned long bootStart = millis();
    while (millis() - bootStart < BOOT_WINDOW_MS) {
        button.update();
        if (button.isPressed() &&
            button.getPressDuration() >= LONG_PRESS_MS) {
            bootLongPress = true;
            break;
        }
        blinkLed(LED_BLINK_BOOT);
        delay(10);
        yield();
    }

    inRecovery = crashReset || rtcRecovery || bootLongPress;
    Serial.printf("[Boot] crash=%d rtcFlag=%d btnHold=%d  =>  recovery=%d\n",
                  crashReset, rtcRecovery, bootLongPress, inRecovery);

    GUI.begin();
    dash.begin(DASH_INT_MS);

    const char* apName = inRecovery
                         ? "LED-CLOCK"
                         : configManager.data.projectName;
    WiFiManager.begin(apName, 15000);

    if (!inRecovery) {
        const char* tz =
            (strlen(configManager.data.timezone) > 0)
            ? configManager.data.timezone
            : "HKT-8";
        timeSync.begin(tz);

        if (timeSync.waitForSyncResult(10000) == 0) {
            ntpSynced = true;
            Serial.println("[NTP] synced");
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
            Serial.println("[NTP] timeout — using manual time");
        }
        fetchWeather();
    }

    lastUserActivity   = millis();
    lastNtpSync        = millis();
    lastWeatherSync    = millis();
    lastDisplayRefresh = 0;

    digitalWrite(STATUS_LED_PIN, HIGH);
    Serial.println("[Boot] complete");
}

// ─────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────
void loop() {
    WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();

    unsigned long now = millis();

    button.update();

    if (button.isPressed()) {
        lastUserActivity = now;

        if (!inRecovery && !longPressHandled &&
            button.getPressDuration() >= LONG_PRESS_MS)
        {
            longPressHandled = true;
            RTCData rtc = { RTC_MAGIC, 1 };
            ESP.rtcUserMemoryWrite(0,
                reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
            delay(50);
            ESP.restart();
        }
    } else {
        longPressHandled = false;
    }

    if (GUI.ws.count() > 0)
        lastUserActivity = now;

    bool isIdle = (now - lastUserActivity > IDLE_TIMEOUT_MS);

    // ── Display ──
    if (inRecovery) {
        drawScrollText("RECOVERY", C_ORANGE);
        flushDisplay();
        delay(1);
    } else {
        unsigned long interval = isIdle ? IDLE_REFRESH_MS : ACTIVE_REFRESH_MS;
        if (now - lastDisplayRefresh >= interval) {
            lastDisplayRefresh = now;
            clearDisplay();
            struct tm* t = getTime();
            if (t) {
                drawTime(t->tm_hour, t->tm_min, C_WHITE);
                int wday = (t->tm_wday == 0) ? 7 : t->tm_wday;
                drawInfoBars(t->tm_mday, wday);
            }
            flushDisplay();
        }
    }

    // ── Dashboard ──
    if (now - lastDashUpdate >= (unsigned long)DASH_INT_MS) {
        lastDashUpdate = now;
        updateDashboard();
    }

    // ── NTP re-sync ──
    if (!inRecovery && now - lastNtpSync >= NTP_INTERVAL_MS) {
        lastNtpSync = now;
        if (timeSync.waitForSyncResult(5000) == 0)
            ntpSynced = true;
    }

    // ── Weather re-fetch ──
    if (!inRecovery && now - lastWeatherSync >= WEATHER_INT_MS) {
        lastWeatherSync = now;
        fetchWeather();
    }

    // ── Status LED ──
    updateStatusLed();

    yield();
}
