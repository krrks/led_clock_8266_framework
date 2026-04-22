// main.cpp — LED Matrix Clock v1.005
// Wemos D1 Mini (ESP8266) + 32×8 WS2812B + 3× button + onboard LED
//
// Changes from v1.004:
//  - 3 buttons: D1(MODE/SELECT), D5(UP/BRIGHTER), D6(DOWN/DIMMER)
//  - FIXED: single/double-click events now consumed via getEvent()
//    Previously getEvent() was never called → clicks had zero effect
//  - Long-press via direct isPressed()+getPressDuration() polling,
//    bypassing ButtonHandler's broken static-bool reset bug
//  - 4 display modes (BTN_MODE click cycles): CLOCK / IP / DATE / TEMP
//  - WiFi connect → auto show IP for 8 s
//  - Heartbeat Serial log every 60 s
//  - Weather on/off toggle (BTN_DOWN long press)
//  - Brightness up/down via dedicated BTN_UP/BTN_DOWN

#include <Arduino.h>
#include "LittleFS.h"
#include <FastLED.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
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
#define BOOT_WINDOW_MS      3000UL   // hold BTN1 during this to enter recovery
#define LONG_PRESS_MS       1000UL   // 1 s long press threshold
#define VERY_LONG_PRESS_MS  5000UL   // 5 s very-long press → recovery
#define IDLE_TIMEOUT_MS    30000UL   // no activity → idle refresh rate
#define ACTIVE_REFRESH_MS   1000UL   // clock redraw when active (1 s)
#define IDLE_REFRESH_MS    30000UL   // clock redraw when idle (30 s)
#define SCROLL_FRAME_MS       60UL   // scroll advance period (~16 fps)
#define NTP_INTERVAL_MS  3600000UL   // NTP re-sync every 60 min
#define WEATHER_INT_MS   3600000UL   // weather re-fetch every 60 min
#define DASH_INT_MS          5000    // dashboard push every 5 s
#define LED_BLINK_BOOT        100UL  // fast blink during boot window
#define LED_BLINK_REC        1000UL  // slow blink in recovery mode
#define WEATHER_FAIL_LIMIT      5    // consecutive HTTP failures → recovery
#define HEARTBEAT_MS        60000UL  // Serial heartbeat every 60 s
#define IP_SHOW_MS           8000UL  // auto-show IP after WiFi connect

// ─────────────────────────────────────────────────────────────────────
// RTC recovery flag
// ─────────────────────────────────────────────────────────────────────
#define RTC_MAGIC 0xC10CFA11UL
struct RTCData { uint32_t magic; uint32_t enterRecovery; };

// ─────────────────────────────────────────────────────────────────────
// Brightness: config stores index 0/1/2 → map to PWM 25/128/255
// ─────────────────────────────────────────────────────────────────────
static const uint8_t BRIGHTNESS_MAP[3] = { 25, 128, 255 };
static inline uint8_t mapBrightness(uint8_t idx) {
    return BRIGHTNESS_MAP[(idx < 3) ? idx : 1];
}

// ─────────────────────────────────────────────────────────────────────
// Display orientation
// ─────────────────────────────────────────────────────────────────────
static uint8_t currentOrientation = 0;

static void applyOrientation(uint32_t src[MATRIX_HEIGHT][MATRIX_WIDTH],
                              uint32_t dst[MATRIX_HEIGHT][MATRIX_WIDTH]) {
    for (int r = 0; r < MATRIX_HEIGHT; r++)
        for (int c = 0; c < MATRIX_WIDTH; c++) {
            uint32_t px;
            switch (currentOrientation) {
                case 2: px = src[MATRIX_HEIGHT-1-r][MATRIX_WIDTH-1-c]; break;
                case 4: px = src[r][MATRIX_WIDTH-1-c];                 break;
                case 5: px = src[MATRIX_HEIGHT-1-r][c];                break;
                default:px = src[r][c];                                break;
            }
            dst[r][c] = px;
        }
}

// ─────────────────────────────────────────────────────────────────────
// Display modes
// ─────────────────────────────────────────────────────────────────────
enum DisplayMode : uint8_t {
    DM_CLOCK = 0,   // HH:MM + info bars
    DM_IP,          // scrolling WiFi IP address
    DM_DATE,        // static date  e.g. "22 APR"
    DM_TEMP,        // static temperature e.g. "25.1C"
    DM_COUNT
};

// ─────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────
static CRGB     leds[NUM_LEDS];
static uint32_t displayMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];

static ButtonHandler btnMode(BUTTON_1_PIN);
static ButtonHandler btnUp  (BUTTON_2_PIN);
static ButtonHandler btnDown(BUTTON_3_PIN);

static uint8_t displayMode    = DM_CLOCK;
static bool    inRecovery     = false;
static bool    ntpSynced      = false;
static bool    weatherEnabled = true;

static int16_t weatherCode      = 0;
static float   weatherTemp      = 0.0f;
static char    weatherDesc[32]  = "N/A";
static int     weatherFailCount = 0;

static struct tm manualTm   = {};
static uint32_t  manualBase = 0;

static unsigned long lastDisplayRefresh = 0;
static unsigned long lastNtpSync        = 0;
static unsigned long lastWeatherSync    = 0;
static unsigned long lastDashUpdate     = 0;
static unsigned long lastUserActivity   = 0;
static unsigned long lastLedBlink       = 0;
static unsigned long lastHeartbeat      = 0;
static unsigned long showIPUntil        = 0;
static bool          ledBlinkState      = false;

static int scrollOff = 0;  // shared scroll position, reset on mode change

// Per-button long-press flags — direct polling avoids ButtonHandler static bug
static bool modeLongHandled  = false;
static bool modeVLongHandled = false;
static bool upLongHandled    = false;
static bool dnLongHandled    = false;

// ─────────────────────────────────────────────────────────────────────
// Colour helpers
// ─────────────────────────────────────────────────────────────────────
static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static const uint32_t C_WHITE  = 0xFFFFFF;
static const uint32_t C_ORANGE = 0xFF9900;
static const uint32_t C_CYAN   = 0x00C8C8;
static const uint32_t C_GREEN  = 0x00CC00;
static const uint32_t C_YELLOW = 0xDDDD00;

// OWM condition code → display colour
static uint32_t wxColor(int16_t code) {
    if (code >= 200 && code < 300) return rgb(160,  0, 200); // thunderstorm: purple
    if (code >= 300 && code < 400) return rgb( 80, 140, 255); // drizzle: light blue
    if (code >= 500 && code < 600) return rgb(  0,  60, 255); // rain: blue
    if (code >= 600 && code < 700) return rgb(200, 220, 255); // snow: icy blue
    if (code >= 700 && code < 800) return rgb(120, 120, 120); // fog: grey
    if (code == 800)               return rgb(255, 200,   0); // clear: yellow
    if (code > 800 && code < 803)  return rgb(200, 200,  80); // few clouds: pale yellow
    if (code >= 803)               return rgb(150, 150, 150); // overcast: grey
    return C_WHITE;
}

// ─────────────────────────────────────────────────────────────────────
// Display primitives
// ─────────────────────────────────────────────────────────────────────
static void clearDisplay() {
    memset(displayMatrix, 0, sizeof(displayMatrix));
}

// Draw one character; returns its pixel width (caller adds +1 gap)
static int drawChar(char c, int x, uint32_t color) {
    const uint8_t* fd = getCharFontData(c);
    uint8_t w = getCharWidth(c);
    if (!fd) return (int)w;
    for (uint8_t col = 0; col < w; col++) {
        int px = x + (int)col;
        if (px < 0 || px >= (int)MATRIX_WIDTH) continue;
        for (uint8_t row = 0; row < DIGIT_HEIGHT; row++)
            if ((fd[col] >> row) & 1)
                displayMatrix[row][px] = color;
    }
    return (int)w;
}

// Pixel width of a string, no trailing gap on last character
static int strPxW(const char* s) {
    int w = 0;
    for (int i = 0; s[i]; i++) w += (int)getCharWidth(s[i]) + 1;
    return (w > 0) ? w - 1 : 0;
}

// Draw string at x; returns x position after the last character + gap
static int drawStr(const char* s, int x, uint32_t color) {
    for (int i = 0; s[i]; i++) x += drawChar(s[i], x, color) + 1;
    return x;
}

// Scrolling text — advances scrollOff on each call.
// Must be called at SCROLL_FRAME_MS rate; clearDisplay() must precede this.
static void drawScroll(const char* txt, uint32_t color) {
    int txtW   = strPxW(txt);
    int totalW = txtW + (int)MATRIX_WIDTH + 8; // 8-px gap between repeats
    int x      = (int)MATRIX_WIDTH - scrollOff;
    for (int i = 0; txt[i]; i++) x += drawChar(txt[i], x, color) + 1;
    if (++scrollOff >= totalW) scrollOff = 0;
}

// ─────────────────────────────────────────────────────────────────────
// Clock face  (DM_CLOCK)
//   Col 0-4   H tens
//   Col 6-10  H units
//   Col 12    colon
//   Col 14-18 M tens
//   Col 20-24 M units
//   Col 26    DATE bar  (1 px / week of month, white, bottom→top)
//   Col 28    WEEKDAY bar  (Mon=1..Sun=7 px, cyan / orange on weekend)
//   Col 30    WEATHER bar  (4 px, colour = OWM condition)
// ─────────────────────────────────────────────────────────────────────
static void drawClockFace() {
    struct tm t = {};
    bool      gotTime = false;

    if (ntpSynced) {
        time_t now = time(nullptr);
        struct tm* p = localtime(&now);
        if (p) { t = *p; gotTime = true; }
    }
    if (!gotTime && manualBase > 0) {
        time_t ts = mktime(&manualTm) + (long)((millis() - manualBase) / 1000UL);
        struct tm* p = localtime(&ts);
        if (p) { t = *p; gotTime = true; }
    }
    if (!gotTime) return;

    // HH:MM digits
    int hr = configManager.data.use24h ? t.tm_hour : (t.tm_hour % 12 ?: 12);
    char d[5];
    snprintf(d, sizeof(d), "%02d%02d", hr, t.tm_min);

    int x = 0;
    x += drawChar(d[0], x, C_WHITE) + 1;  // col 0
    x += drawChar(d[1], x, C_WHITE) + 1;  // col 6
    x += drawChar(':',  x, C_WHITE) + 1;  // col 12
    x += drawChar(d[2], x, C_WHITE) + 1;  // col 14
         drawChar(d[3], x, C_WHITE);       // col 20

    // Info bars — bottom row first (MATRIX_HEIGHT-1 = row 7)
    int mday = t.tm_mday;
    int wday = (t.tm_wday == 0) ? 7 : t.tm_wday; // 1=Mon..7=Sun

    // DATE bar (col 26): 1 px per 7-day week of month
    int wom = constrain((mday - 1) / 7 + 1, 1, 5);
    for (int i = 0; i < wom; i++)
        displayMatrix[MATRIX_HEIGHT - 1 - i][26] = C_WHITE;

    // WEEKDAY bar (col 28): orange on weekend, cyan on weekday
    uint32_t wdCol = (wday >= 6) ? C_ORANGE : C_CYAN;
    for (int i = 0; i < wday; i++)
        displayMatrix[MATRIX_HEIGHT - 1 - i][28] = wdCol;

    // WEATHER bar (col 30): 4 px, colour = OWM condition
    if (weatherEnabled && weatherCode > 0) {
        uint32_t wxc = wxColor(weatherCode);
        for (int i = 0; i < 4; i++)
            displayMatrix[MATRIX_HEIGHT - 1 - i][30] = wxc;
    }
}

// ─────────────────────────────────────────────────────────────────────
// IP address face  (DM_IP) — scrolling "IP 192.168.x.x"
// ─────────────────────────────────────────────────────────────────────
static void drawIPFace() {
    static char    ipBuf[48]        = "IP NO WIFI";
    static unsigned long ipUpdated  = 0;

    if (millis() - ipUpdated > 5000) {
        ipUpdated = millis();
        if (WiFi.status() == WL_CONNECTED) {
            String s = WiFi.localIP().toString();
            snprintf(ipBuf, sizeof(ipBuf), "IP %s", s.c_str());
        } else {
            strlcpy(ipBuf, "IP NO WIFI", sizeof(ipBuf));
        }
    }
    drawScroll(ipBuf, C_CYAN);
}

// ─────────────────────────────────────────────────────────────────────
// Date face  (DM_DATE) — static "22 APR"
// ─────────────────────────────────────────────────────────────────────
static void drawDateFace() {
    static const char* months[] = {
        "JAN","FEB","MAR","APR","MAY","JUN",
        "JUL","AUG","SEP","OCT","NOV","DEC"
    };
    struct tm t = {};
    bool      gotTime = false;

    if (ntpSynced) {
        time_t now = time(nullptr);
        struct tm* p = localtime(&now);
        if (p) { t = *p; gotTime = true; }
    }
    if (!gotTime && manualBase > 0) {
        time_t ts = mktime(&manualTm) + (long)((millis() - manualBase) / 1000UL);
        struct tm* p = localtime(&ts);
        if (p) { t = *p; gotTime = true; }
    }

    char s[12];
    if (gotTime) snprintf(s, sizeof(s), "%02d %s", t.tm_mday, months[t.tm_mon]);
    else         strlcpy(s, "NO DATE", sizeof(s));

    int w = strPxW(s);
    int startX = ((int)MATRIX_WIDTH - w) / 2;
    if (startX < 0) startX = 0;
    drawStr(s, startX, C_GREEN);
}

// ─────────────────────────────────────────────────────────────────────
// Temperature face  (DM_TEMP) — static "25.1C" or "WX OFF"
// ─────────────────────────────────────────────────────────────────────
static void drawTempFace() {
    char s[12];
    if (weatherEnabled && weatherCode > 0)
        snprintf(s, sizeof(s), "%.1fC", weatherTemp);
    else
        strlcpy(s, "WX OFF", sizeof(s));

    int w = strPxW(s);
    int startX = ((int)MATRIX_WIDTH - w) / 2;
    if (startX < 0) startX = 0;
    drawStr(s, startX, C_YELLOW);
}

// ─────────────────────────────────────────────────────────────────────
// Push logical matrix → physical LED strip
// ─────────────────────────────────────────────────────────────────────
static void flushDisplay() {
    uint32_t oriented[MATRIX_HEIGHT][MATRIX_WIDTH];
    applyOrientation(displayMatrix, oriented);

    uint32_t buf[NUM_LEDS];
    convertToSnakeOrder(oriented, buf);

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].r = (uint8_t)((buf[i] >> 16) & 0xFF);
        leds[i].g = (uint8_t)((buf[i] >>  8) & 0xFF);
        leds[i].b = (uint8_t)( buf[i]        & 0xFF);
    }
    FastLED.show();
}

// ─────────────────────────────────────────────────────────────────────
// Recovery trigger — write RTC flag then restart
// ─────────────────────────────────────────────────────────────────────
static void triggerRecovery() {
    Serial.println("[Recovery] writing RTC flag and restarting...");
    RTCData rtc = { RTC_MAGIC, 1 };
    ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
    delay(50);
    ESP.restart();
}

// ─────────────────────────────────────────────────────────────────────
// Weather fetch (plain HTTP, OWM free tier)
// ─────────────────────────────────────────────────────────────────────
static void fetchWeather() {
    if (!weatherEnabled) {
        Serial.println("[Weather] disabled — skipping");
        return;
    }
    if (WiFiManager.isCaptivePortal()) return;

    const char* city   = configManager.data.weatherCity;
    const char* apiKey = configManager.data.weatherApiKey;
    if (!city || !apiKey || strlen(city) == 0 || strlen(apiKey) < 8) {
        Serial.println("[Weather] skip — city or API key not configured");
        return;
    }

    WiFiClient client;
    HTTPClient  http;
    String url = F("http://api.openweathermap.org/data/2.5/weather?q=");
    url += city;
    url += F("&appid=");
    url += apiKey;
    url += F("&units=metric");

    Serial.printf("[Weather] fetching city=%s\n", city);
    http.setTimeout(8000);

    bool success = false;
    bool cfgError = false;

    if (http.begin(client, url)) {
        int code = http.GET();

        if (code == HTTP_CODE_OK) {
            StaticJsonDocument<1024> doc;
            DeserializationError err = deserializeJson(doc, http.getStream());
            if (!err) {
                weatherCode = (int16_t)(doc["weather"][0]["id"] | 0);
                weatherTemp = doc["main"]["temp"] | 0.0f;
                const char* desc = doc["weather"][0]["description"] | "Unknown";
                strlcpy(weatherDesc, desc, sizeof(weatherDesc));
                success = true;
                weatherFailCount = 0;
                Serial.printf("[Weather] OK  code=%d  %.1f°C  %s\n",
                              weatherCode, weatherTemp, weatherDesc);
            } else {
                Serial.printf("[Weather] JSON error: %s\n", err.c_str());
            }
        } else if (code == 401) {
            Serial.println("[Weather] ERROR 401 — API key invalid/not activated");
            Serial.println("          Fix: Configuration > API Key");
            cfgError = true;
        } else if (code == 404) {
            Serial.printf("[Weather] ERROR 404 — city not found: \"%s\"\n", city);
            Serial.println("          Fix: Configuration > City");
            cfgError = true;
        } else if (code == 400) {
            Serial.printf("[Weather] ERROR 400 — bad request (check city: \"%s\")\n", city);
            cfgError = true;
        } else {
            Serial.printf("[Weather] HTTP error %d\n", code);
        }
        http.end();
    } else {
        Serial.println("[Weather] http.begin() failed");
    }

    if (!success && !cfgError) {
        weatherFailCount++;
        Serial.printf("[Weather] fail count %d / %d\n", weatherFailCount, WEATHER_FAIL_LIMIT);
        if (weatherFailCount >= WEATHER_FAIL_LIMIT) {
            Serial.printf("[Weather] %d consecutive failures — triggering recovery\n",
                          weatherFailCount);
            triggerRecovery();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────
// Dashboard push
// ─────────────────────────────────────────────────────────────────────
static void updateDashboard() {
    struct tm t = {};
    bool gotTime = false;

    if (ntpSynced) {
        time_t now = time(nullptr);
        struct tm* p = localtime(&now);
        if (p) { t = *p; gotTime = true; }
    }
    if (!gotTime && manualBase > 0) {
        time_t ts = mktime(&manualTm) + (long)((millis() - manualBase) / 1000UL);
        struct tm* p = localtime(&ts);
        if (p) { t = *p; gotTime = true; }
    }
    if (!gotTime) return;

    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    strlcpy(dash.data.currentTime, tmp, sizeof(dash.data.currentTime));

    snprintf(tmp, sizeof(tmp), "%04d-%02d-%02d",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    strlcpy(dash.data.currentDate, tmp, sizeof(dash.data.currentDate));

    static const char* wdn[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    strlcpy(dash.data.weekday, wdn[t.tm_wday], sizeof(dash.data.weekday));

    dash.data.ntpSynced    = ntpSynced;
    dash.data.temperature  = weatherTemp;
    dash.data.weatherCode  = (uint16_t)weatherCode;
    strlcpy(dash.data.weatherDesc, weatherDesc, sizeof(dash.data.weatherDesc));
    dash.data.freeHeap    = (uint32_t)ESP.getFreeHeap();
    dash.data.uptime      = (uint32_t)(millis() / 1000UL);
    dash.data.inRecovery  = inRecovery;
}

// ─────────────────────────────────────────────────────────────────────
// Status LED
// ─────────────────────────────────────────────────────────────────────
static void blinkLED(unsigned long rateMs) {
    unsigned long now = millis();
    if (now - lastLedBlink >= rateMs) {
        lastLedBlink  = now;
        ledBlinkState = !ledBlinkState;
        digitalWrite(STATUS_LED_PIN, ledBlinkState ? LOW : HIGH);
    }
}

static void updateStatusLED() {
    if (inRecovery) blinkLED(LED_BLINK_REC);
    else            digitalWrite(STATUS_LED_PIN, HIGH); // OFF (active LOW)
}

// ─────────────────────────────────────────────────────────────────────
// Heartbeat serial log
// ─────────────────────────────────────────────────────────────────────
static const char* dmodeName(uint8_t m) {
    switch (m) {
        case DM_CLOCK: return "clock";
        case DM_IP:    return "ip";
        case DM_DATE:  return "date";
        case DM_TEMP:  return "temp";
        default:       return "?";
    }
}

static void printHeartbeat() {
    struct tm t = {};
    bool gotTime = false;

    if (ntpSynced) {
        time_t now = time(nullptr);
        struct tm* p = localtime(&now);
        if (p) { t = *p; gotTime = true; }
    }
    if (!gotTime && manualBase > 0) {
        time_t ts = mktime(&manualTm) + (long)((millis() - manualBase) / 1000UL);
        struct tm* p = localtime(&ts);
        if (p) { t = *p; gotTime = true; }
    }

    char timeBuf[12] = "--:--:--";
    if (gotTime)
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
                 t.tm_hour, t.tm_min, t.tm_sec);

    String wifiInfo = WiFiManager.isCaptivePortal() ? "AP:192.168.4.1" :
                      (WiFi.status() == WL_CONNECTED
                       ? WiFi.localIP().toString()
                       : "offline");

    Serial.printf("[Heartbeat] %s | heap=%uB | ntp=%d | "
                  "wx=%d %.1fC [%s] | mode=%s | bright=%d | "
                  "orient=%d | wifi=%s\n",
                  timeBuf,
                  ESP.getFreeHeap(),
                  ntpSynced,
                  weatherCode, weatherTemp,
                  weatherEnabled ? "on" : "off",
                  dmodeName(displayMode),
                  configManager.data.brightness,
                  currentOrientation,
                  wifiInfo.c_str());
}

// ─────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println(F("\n================================"));
    Serial.println(F("  LED Matrix Clock  v1.005"));
    Serial.println(F("================================"));
    Serial.printf("[Sys] Chip ID   : %08X\n",  ESP.getChipId());
    Serial.printf("[Sys] SDK       : %s\n",     ESP.getSdkVersion());
    Serial.printf("[Sys] Flash     : %u KB\n",  ESP.getFlashChipSize() / 1024);
    Serial.printf("[Sys] Free heap : %u B\n",   ESP.getFreeHeap());
    Serial.printf("[Sys] CPU freq  : %u MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("[Sys] Reset     : %s\n",     ESP.getResetReason().c_str());

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW); // LED on during boot

    // ── Button init ───────────────────────────────────────────────────
    btnMode.begin();
    btnUp.begin();
    btnDown.begin();
    Serial.printf("[Boot] Buttons: MODE=GPIO%d  UP=GPIO%d  DOWN=GPIO%d\n",
                  BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN);

    // ── Framework ─────────────────────────────────────────────────────
    LittleFS.begin();
    configManager.begin();
    Serial.println("[Boot] Config loaded");

    // ── FastLED init ──────────────────────────────────────────────────
    FastLED.addLeds<WS2812B, LED_MATRIX_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(mapBrightness(configManager.data.brightness));
    FastLED.clear(true);
    currentOrientation = configManager.data.displayOrientation;

    Serial.printf("[Boot] Brightness level %d (raw=%d) | Orientation %d\n",
                  configManager.data.brightness,
                  mapBrightness(configManager.data.brightness),
                  currentOrientation);

    // Re-apply display settings whenever user saves from web UI
    configManager.setConfigSaveCallback([]() {
        FastLED.setBrightness(mapBrightness(configManager.data.brightness));
        currentOrientation = configManager.data.displayOrientation;
        Serial.printf("[Config saved] brightness=%d (raw=%d) orientation=%d\n",
                      configManager.data.brightness,
                      mapBrightness(configManager.data.brightness),
                      currentOrientation);
        lastUserActivity = millis();
    });

    // ── Recovery detection 1: crash / watchdog ────────────────────────
    String resetReason = ESP.getResetReason();
    bool crashReset = (resetReason == "Exception" ||
                       resetReason.indexOf("Watchdog") >= 0);

    // ── Recovery detection 2: RTC flag from previous long-press ───────
    RTCData rtc = {};
    bool rtcRecovery = false;
    if (ESP.rtcUserMemoryRead(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc))) {
        if (rtc.magic == RTC_MAGIC && rtc.enterRecovery == 1) {
            rtcRecovery       = true;
            rtc.enterRecovery = 0;
            ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
        }
    }

    // ── Boot window: watch BTN_MODE for long press ─────────────────────
    bool bootLongPress = false;
    unsigned long bootStart = millis();
    Serial.printf("[Boot] Boot window %lums — hold BTN1(D1) for recovery\n",
                  BOOT_WINDOW_MS);

    while (millis() - bootStart < BOOT_WINDOW_MS) {
        btnMode.update();
        if (btnMode.isPressed() && btnMode.getPressDuration() >= LONG_PRESS_MS) {
            bootLongPress = true;
            break;
        }
        blinkLED(LED_BLINK_BOOT);
        delay(10);
        yield();
    }

    inRecovery = crashReset || rtcRecovery || bootLongPress;
    Serial.printf("[Boot] crash=%d rtcFlag=%d btnHold=%d  =>  recovery=%d\n",
                  crashReset, rtcRecovery, bootLongPress, inRecovery);

    // ── Framework services ────────────────────────────────────────────
    GUI.begin();
    dash.begin(DASH_INT_MS);

    const char* apName = inRecovery
                         ? "LED-CLOCK"
                         : configManager.data.projectName;
    WiFiManager.begin(apName, 15000);

    bool wifiOK = (WiFi.status() == WL_CONNECTED && !WiFiManager.isCaptivePortal());

    // ── NTP + weather (normal mode only) ─────────────────────────────
    if (!inRecovery) {
        const char* tz = (strlen(configManager.data.timezone) > 0)
                         ? configManager.data.timezone : "HKT-8";
        Serial.printf("[NTP] timezone = %s\n", tz);
        timeSync.begin(tz);

        if (timeSync.waitForSyncResult(10000) == 0) {
            ntpSynced = true;
            time_t now = time(nullptr);
            char tbuf[32];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
            Serial.printf("[NTP] synced — %s\n", tbuf);
        } else {
            // Manual time fallback
            memset(&manualTm, 0, sizeof(manualTm));
            manualTm.tm_hour  = configManager.data.manualHour;
            manualTm.tm_min   = configManager.data.manualMinute;
            manualTm.tm_mday  = configManager.data.manualDay;
            manualTm.tm_mon   = configManager.data.manualMonth - 1;
            manualTm.tm_year  = configManager.data.manualYear - 1900;
            manualTm.tm_isdst = -1;
            mktime(&manualTm);
            manualBase = millis();
            Serial.printf("[NTP] timeout — using manual time %02d:%02d\n",
                          configManager.data.manualHour,
                          configManager.data.manualMinute);
        }
        fetchWeather();
    }

    // ── Auto-show IP after WiFi connect ──────────────────────────────
    if (wifiOK) {
        Serial.printf("[WiFi] connected — IP %s\n",
                      WiFi.localIP().toString().c_str());
        displayMode = DM_IP;
        showIPUntil = millis() + IP_SHOW_MS;
        scrollOff   = 0;
    }

    // ── Init timers ───────────────────────────────────────────────────
    lastUserActivity   = millis();
    lastNtpSync        = millis();
    lastWeatherSync    = millis();
    lastDisplayRefresh = 0;
    lastHeartbeat      = millis();

    digitalWrite(STATUS_LED_PIN, HIGH); // LED off (active LOW)
    Serial.printf("[Boot] complete — heap=%uB\n", ESP.getFreeHeap());
    Serial.println(F("================================\n"));
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

    // ── Update all three buttons (call first, every loop iteration) ──
    btnMode.update();
    btnUp.update();
    btnDown.update();

    // ── Auto-return from IP mode ──────────────────────────────────────
    if (showIPUntil > 0 && now >= showIPUntil) {
        showIPUntil = 0;
        if (displayMode == DM_IP) {
            displayMode = DM_CLOCK;
            scrollOff   = 0;
            lastDisplayRefresh = 0;
            Serial.println("[IP] auto-return to clock mode");
        }
    }

    // ── BTN_MODE: click → cycle display mode ─────────────────────────
    // getEvent() consumes the event (fix: previously never called)
    ButtonEvent evtMode = btnMode.getEvent();

    if (evtMode == BE_CLICK) {
        displayMode = (displayMode + 1) % DM_COUNT;
        showIPUntil = 0;   // cancel any pending auto-return
        scrollOff   = 0;
        lastDisplayRefresh = 0;
        lastUserActivity   = now;
        Serial.printf("[Button] MODE click — display mode: %s\n",
                      dmodeName(displayMode));
    } else if (evtMode == BE_DOUBLE_CLICK) {
        lastUserActivity = now;
        Serial.println("[Button] MODE double-click — force NTP + weather refresh");
        if (!inRecovery) {
            if (timeSync.waitForSyncResult(3000) == 0) ntpSynced = true;
            lastNtpSync     = now;
            fetchWeather();
            lastWeatherSync = now;
        }
    }

    // BTN_MODE long press (direct polling; avoids ButtonHandler static bug)
    if (btnMode.isPressed()) {
        lastUserActivity = now;
        uint32_t dur = btnMode.getPressDuration();

        if (!modeVLongHandled && dur >= VERY_LONG_PRESS_MS) {
            modeVLongHandled = true;
            Serial.println("[Button] MODE 5 s — entering recovery");
            triggerRecovery();

        } else if (!modeLongHandled && dur >= LONG_PRESS_MS) {
            modeLongHandled = true;
            Serial.println("[Button] MODE 1 s — showing IP address");
            displayMode = DM_IP;
            showIPUntil = now + IP_SHOW_MS;
            scrollOff   = 0;
        }
    } else {
        modeLongHandled  = false;
        modeVLongHandled = false;
    }

    // ── BTN_UP: click → brightness +1 ────────────────────────────────
    ButtonEvent evtUp = btnUp.getEvent();

    if (evtUp == BE_CLICK) {
        lastUserActivity = now;
        uint8_t b = (configManager.data.brightness + 1) % 3;
        configManager.data.brightness = b;
        configManager.save();
        FastLED.setBrightness(mapBrightness(b));
        Serial.printf("[Button] UP click — brightness %d (raw=%d)\n",
                      b, mapBrightness(b));
    }

    // BTN_UP long press → force NTP + weather refresh
    if (btnUp.isPressed()) {
        lastUserActivity = now;
        if (!upLongHandled && btnUp.getPressDuration() >= LONG_PRESS_MS) {
            upLongHandled = true;
            Serial.println("[Button] UP 1 s — force NTP + weather refresh");
            if (!inRecovery) {
                if (timeSync.waitForSyncResult(3000) == 0) ntpSynced = true;
                lastNtpSync     = now;
                fetchWeather();
                lastWeatherSync = now;
            }
        }
    } else {
        upLongHandled = false;
    }

    // ── BTN_DOWN: click → brightness -1 ──────────────────────────────
    ButtonEvent evtDown = btnDown.getEvent();

    if (evtDown == BE_CLICK) {
        lastUserActivity = now;
        uint8_t b = (configManager.data.brightness == 0)
                    ? 2 : configManager.data.brightness - 1;
        configManager.data.brightness = b;
        configManager.save();
        FastLED.setBrightness(mapBrightness(b));
        Serial.printf("[Button] DOWN click — brightness %d (raw=%d)\n",
                      b, mapBrightness(b));
    }

    // BTN_DOWN long press → toggle weather on/off
    if (btnDown.isPressed()) {
        lastUserActivity = now;
        if (!dnLongHandled && btnDown.getPressDuration() >= LONG_PRESS_MS) {
            dnLongHandled = true;
            weatherEnabled = !weatherEnabled;
            Serial.printf("[Button] DOWN 1 s — weather %s\n",
                          weatherEnabled ? "ENABLED" : "DISABLED");
        }
    } else {
        dnLongHandled = false;
    }

    // ── WebSocket client activity (keeps out of idle) ─────────────────
    if (GUI.ws.count() > 0) lastUserActivity = now;
    bool isIdle = (now - lastUserActivity > IDLE_TIMEOUT_MS);

    // ── Display render ────────────────────────────────────────────────
    {
        unsigned long interval;
        if (inRecovery || displayMode == DM_IP) {
            // Scrolling modes need frequent update
            interval = SCROLL_FRAME_MS;
        } else {
            // Static modes: fast when active, slow when idle
            interval = isIdle ? IDLE_REFRESH_MS : ACTIVE_REFRESH_MS;
        }

        if (now - lastDisplayRefresh >= interval) {
            lastDisplayRefresh = now;
            clearDisplay();

            if (inRecovery) {
                drawScroll("RECOVERY", C_ORANGE);
            } else {
                switch (displayMode) {
                    case DM_CLOCK: drawClockFace(); break;
                    case DM_IP:    drawIPFace();    break;
                    case DM_DATE:  drawDateFace();  break;
                    case DM_TEMP:  drawTempFace();  break;
                }
            }
            flushDisplay();
        }
    }

    // ── Dashboard push ────────────────────────────────────────────────
    if (now - lastDashUpdate >= (unsigned long)DASH_INT_MS) {
        lastDashUpdate = now;
        updateDashboard();
    }

    // ── Serial heartbeat (every 60 s) ─────────────────────────────────
    if (now - lastHeartbeat >= HEARTBEAT_MS) {
        lastHeartbeat = now;
        printHeartbeat();
    }

    // ── NTP periodic re-sync ──────────────────────────────────────────
    if (!inRecovery && now - lastNtpSync >= NTP_INTERVAL_MS) {
        lastNtpSync = now;
        Serial.println("[NTP] periodic re-sync...");
        if (timeSync.waitForSyncResult(5000) == 0) {
            ntpSynced = true;
            Serial.println("[NTP] re-sync OK");
        } else {
            Serial.println("[NTP] re-sync timeout (using millis drift)");
        }
    }

    // ── Weather periodic re-fetch ─────────────────────────────────────
    if (!inRecovery && weatherEnabled && now - lastWeatherSync >= WEATHER_INT_MS) {
        lastWeatherSync = now;
        fetchWeather();
    }

    // ── Status LED ────────────────────────────────────────────────────
    updateStatusLED();

    yield();
}
