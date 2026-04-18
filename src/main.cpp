// main.cpp — LED Matrix Clock  v1.2.0
//
// Changes v1.2.0:
//   - Serial boot banner (version, chip ID, heap, reset reason)
//   - Periodic heartbeat serial output every 60 s
//   - Button single-click: cycle brightness (Dim → Medium → Bright → Dim)
//   - Button double-click: force NTP + weather refresh immediately
//   - Button 5 s hold: reboot device
//   - Display orientation transform (Normal / 90°CW / 180° / 270°CW / H-Flip / V-Flip)
//   - Brightness config changed to 3-level select (0=Dim 1=Med 2=Bright)

#include <Arduino.h>
#include "LittleFS.h"
#include <FastLED.h>
#include <ArduinoJson.h>
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
// Version & compile-time constants
// ─────────────────────────────────────────────────────────────────────
#define FIRMWARE_VERSION    "1.2.0"
#define BOOT_WINDOW_MS     3000UL
#define LONG_PRESS_MS      1000UL
#define IDLE_TIMEOUT_MS   30000UL
#define ACTIVE_REFRESH_MS  1000UL
#define IDLE_REFRESH_MS   30000UL
#define SCROLL_FRAME_MS     80UL
#define NTP_INTERVAL_MS  3600000UL
#define WEATHER_INT_MS   3600000UL
#define DASH_INT_MS        5000
#define LED_BLINK_BOOT      100UL
#define LED_BLINK_REC      1000UL
#define HEARTBEAT_INTERVAL 60000UL

// Brightness presets: config value 0/1/2 → FastLED raw level
static const uint8_t BRT_LEVELS[3] = { 10, 60, 180 };

// RTC memory layout (two uint32_t = 8 bytes, required multiple-of-4)
#define RTC_MAGIC 0xC10CFA11UL
struct RTCData {
    uint32_t magic;
    uint32_t enterRecovery;  // 1 = enter recovery on next boot
};

// ─────────────────────────────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────────────────────────────
CRGB          leds[NUM_LEDS];
uint32_t      displayMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];
ButtonHandler button(BUTTON_PIN);

bool inRecovery       = false;
bool ntpSynced        = false;
bool longPressHandled = false;
bool veryLongHandled  = false;

int16_t  weatherCode      = 800;
float    weatherTemp      = 25.0f;
char     weatherDesc[32]  = "Clear";

struct tm manualTm   = {};
uint32_t  manualBase = 0;

unsigned long lastDisplayRefresh = 0;
unsigned long lastNtpSync        = 0;
unsigned long lastWeatherSync    = 0;
unsigned long lastDashUpdate     = 0;
unsigned long lastUserActivity   = 0;
unsigned long lastLedBlink       = 0;
unsigned long lastHeartbeat      = 0;
bool          ledBlinkState      = false;

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
    if (code >= 200 && code < 300) return rgb(128,   0, 128); // thunderstorm: purple
    if (code >= 300 && code < 400) return rgb(  0, 128, 255); // drizzle: light blue
    if (code >= 500 && code < 600) return rgb(  0,   0, 255); // rain: blue
    if (code >= 600 && code < 700) return rgb(200, 200, 255); // snow: pale cyan
    if (code >= 700 && code < 800) return rgb(128, 128, 128); // fog/mist: grey
    if (code == 800)               return rgb(255, 220,   0); // clear: yellow
    if (code  > 800)               return rgb(180, 180, 180); // clouds: grey
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

// Scrolling recovery text (non-blocking: advances offset on SCROLL_FRAME_MS cadence)
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

// Draw HH:MM centred in the 32-column display (25 px wide + 3-4 px margins).
static void drawTime(int hour, int minute, uint32_t color) {
    int x = 3;
    x += drawChar((char)('0' + hour   / 10), x, color); x++;
    x += drawChar((char)('0' + hour   % 10), x, color); x++;
    x += drawChar(':',                        x, color); x++;
    x += drawChar((char)('0' + minute / 10), x, color); x++;
         drawChar((char)('0' + minute % 10), x, color);
}

// Three info bars on the bottom pixel row.
//   cols  0-4  : week-of-month (white, 1-5 px)
//   cols  6-12 : weekday 1-7 (cyan weekday / orange weekend)
//   cols 14-17 : weather condition colour (4 px)
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

// Apply orientation transform then snake-map displayMatrix → hardware LEDs.
//
// Orientation values (configManager.data.displayOrientation):
//   0 = Normal
//   1 = 90° CW  (content scaled to fill 32×8)
//   2 = 180°    (mount display upside-down)
//   3 = 270° CW (content scaled to fill 32×8)
//   4 = H-Flip  (mirror left ↔ right)
//   5 = V-Flip  (mirror top ↕ bottom)
//
// 90°/270° use backward-mapping with integer scaling so all LEDs are
// populated; the content will appear geometrically distorted for a
// non-square display but the transform is fully implemented.
static void flushDisplay() {
    uint8_t ori = configManager.data.displayOrientation;
    uint32_t oriented[MATRIX_HEIGHT][MATRIX_WIDTH];

    // Backward mapping: for each output pixel (dr, dc) find the source pixel.
    for (uint8_t dr = 0; dr < MATRIX_HEIGHT; dr++) {
        for (uint8_t dc = 0; dc < MATRIX_WIDTH; dc++) {
            uint8_t row = dr, col = dc;
            switch (ori) {
                case 1: // 90° CW — scaled for 32×8 aspect ratio
                    row = (uint8_t)(dc * MATRIX_HEIGHT / MATRIX_WIDTH);
                    col = (uint8_t)((MATRIX_HEIGHT - 1 - dr)
                                    * MATRIX_WIDTH / MATRIX_HEIGHT);
                    break;
                case 2: // 180°
                    row = MATRIX_HEIGHT - 1 - dr;
                    col = MATRIX_WIDTH  - 1 - dc;
                    break;
                case 3: // 270° CW — scaled for 32×8 aspect ratio
                    row = (uint8_t)((MATRIX_WIDTH - 1 - dc)
                                    * MATRIX_HEIGHT / MATRIX_WIDTH);
                    col = (uint8_t)(dr * MATRIX_WIDTH / MATRIX_HEIGHT);
                    break;
                case 4: // H-Flip
                    col = MATRIX_WIDTH - 1 - dc;
                    break;
                case 5: // V-Flip
                    row = MATRIX_HEIGHT - 1 - dr;
                    break;
                default: // Normal — no transform
                    break;
            }
            // Safety clamp (should not trigger under normal operation)
            if (row >= MATRIX_HEIGHT) row = MATRIX_HEIGHT - 1;
            if (col >= MATRIX_WIDTH)  col = MATRIX_WIDTH  - 1;
            oriented[dr][dc] = displayMatrix[row][col];
        }
    }

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
// ─────────────────────────────────────────────────────────────────────
static void fetchWeather() {
    if (WiFiManager.isCaptivePortal()) {
        Serial.println(F("[Weather] skipped — captive portal active"));
        return;
    }
    if (strlen(configManager.data.weatherApiKey) < 8) {
        Serial.println(F("[Weather] skipped — API key not configured"));
        return;
    }

    Serial.printf("[Weather] fetching city=%s...\n",
                  configManager.data.weatherCity);

    WiFiClient client;
    HTTPClient http;

    String url = F("http://api.openweathermap.org/data/2.5/weather?q=");
    url += configManager.data.weatherCity;
    url += F("&appid=");
    url += configManager.data.weatherApiKey;
    url += F("&units=metric");

    if (http.begin(client, url)) {
        int code = http.GET();
        if (code == HTTP_CODE_OK) {
            StaticJsonDocument<1024> doc;
            DeserializationError err = deserializeJson(doc, http.getStream());
            if (!err) {
                weatherCode = (int16_t)(doc["weather"][0]["id"]   | 800);
                weatherTemp =           doc["main"]["temp"]        | 25.0f;
                strlcpy(weatherDesc,
                        doc["weather"][0]["description"] | "Unknown",
                        sizeof(weatherDesc));
                Serial.printf("[Weather] OK  code=%d  %.1f°C  %s\n",
                              weatherCode, weatherTemp, weatherDesc);
            } else {
                Serial.printf("[Weather] JSON error: %s\n", err.c_str());
            }
        } else {
            Serial.printf("[Weather] HTTP error: %d\n", code);
        }
        http.end();
    } else {
        Serial.println(F("[Weather] http.begin() failed"));
    }
}

// ─────────────────────────────────────────────────────────────────────
// Dashboard data fill (struct auto-generated from gui/js/dashboard.json)
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
// Status LED (GPIO2, active LOW)
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
        blinkLed(LED_BLINK_REC);  // slow blink 1 s
    } else {
        digitalWrite(STATUS_LED_PIN, HIGH); // OFF in normal mode
    }
}

// ─────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);

    // ── Boot banner ──────────────────────────────────────────────────
    Serial.println();
    Serial.println(F("================================"));
    Serial.println(F("  LED Matrix Clock  v" FIRMWARE_VERSION));
    Serial.println(F("================================"));
    Serial.printf("[Sys] Chip ID   : %08X\n",  ESP.getChipId());
    Serial.printf("[Sys] SDK       : %s\n",    ESP.getSdkVersion());
    Serial.printf("[Sys] Flash     : %u KB\n", ESP.getFlashChipSize() / 1024);
    Serial.printf("[Sys] Free heap : %u B\n",  ESP.getFreeHeap());
    Serial.printf("[Sys] CPU freq  : %u MHz\n",ESP.getCpuFreqMHz());
    Serial.printf("[Sys] Reset     : %s\n",    ESP.getResetReason().c_str());

    // ── Hardware init ────────────────────────────────────────────────
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);  // ON during boot

    button.begin();
    LittleFS.begin();
    configManager.begin();
    Serial.println(F("[Boot] Config loaded"));

    // ── LED matrix ───────────────────────────────────────────────────
    FastLED.addLeds<WS2812B, LED_MATRIX_PIN, GRB>(leds, NUM_LEDS);
    {
        uint8_t brtIdx = configManager.data.brightness;
        if (brtIdx > 2) brtIdx = 1;
        FastLED.setBrightness(BRT_LEVELS[brtIdx]);
        Serial.printf("[Boot] Brightness level %d (raw=%d) | Orientation %d\n",
                      brtIdx, BRT_LEVELS[brtIdx],
                      configManager.data.displayOrientation);
    }
    FastLED.clear(true);

    // Re-apply brightness & log when user saves config from web UI
    configManager.setConfigSaveCallback([]() {
        uint8_t brtIdx = configManager.data.brightness;
        if (brtIdx > 2) brtIdx = 1;
        FastLED.setBrightness(BRT_LEVELS[brtIdx]);
        lastUserActivity = millis();
        Serial.printf("[Config] Saved — brightness=%d orient=%d tz=%s city=%s\n",
                      brtIdx,
                      configManager.data.displayOrientation,
                      configManager.data.timezone,
                      configManager.data.weatherCity);
    });

    // ── Recovery detection ───────────────────────────────────────────
    String reason     = ESP.getResetReason();
    bool   crashReset = (reason == "Exception" ||
                         reason.indexOf("Watchdog") >= 0);

    RTCData rtc = {};
    bool rtcRecovery = false;
    if (ESP.rtcUserMemoryRead(0,
            reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc))) {
        if (rtc.magic == RTC_MAGIC && rtc.enterRecovery == 1) {
            rtcRecovery       = true;
            rtc.enterRecovery = 0;
            ESP.rtcUserMemoryWrite(0,
                reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
        }
    }

    // Boot window: fast-blink LED and watch for long-press → recovery
    bool bootLongPress = false;
    unsigned long bootStart = millis();
    Serial.printf("[Boot] Boot window %lu ms — hold button for recovery\n",
                  BOOT_WINDOW_MS);
    while (millis() - bootStart < BOOT_WINDOW_MS) {
        button.update();
        if (button.isPressed() &&
            button.getPressDuration() >= LONG_PRESS_MS) {
            bootLongPress = true;
            Serial.println(F("[Boot] Button held — recovery triggered"));
            break;
        }
        blinkLed(LED_BLINK_BOOT);
        delay(10);
        yield();
    }

    inRecovery = crashReset || rtcRecovery || bootLongPress;
    Serial.printf("[Boot] crash=%d rtcFlag=%d btnHold=%d  =>  recovery=%d\n",
                  crashReset, rtcRecovery, bootLongPress, inRecovery);

    // ── Framework init ───────────────────────────────────────────────
    GUI.begin();
    dash.begin(DASH_INT_MS);

    const char* apName = inRecovery ? "LED-CLOCK"
                                    : configManager.data.projectName;
    Serial.printf("[WiFi] connecting... (AP fallback: \"%s\")\n", apName);
    WiFiManager.begin(apName, 15000);

    if (!WiFiManager.isCaptivePortal()) {
        Serial.println(F("[WiFi] connected to existing network"));
    } else {
        Serial.printf("[WiFi] AP mode started: \"%s\"\n", apName);
    }

    // ── NTP + manual time fallback (normal mode only) ────────────────
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
            // Fall back to manually configured time tracked via millis()
            memset(&manualTm, 0, sizeof(manualTm));
            manualTm.tm_hour  = configManager.data.manualHour;
            manualTm.tm_min   = configManager.data.manualMinute;
            manualTm.tm_mday  = configManager.data.manualDay;
            manualTm.tm_mon   = configManager.data.manualMonth - 1;
            manualTm.tm_year  = configManager.data.manualYear - 1900;
            manualTm.tm_isdst = -1;
            mktime(&manualTm);
            manualBase = millis();
            Serial.printf("[NTP] timeout — manual time set to %04d-%02d-%02d %02d:%02d\n",
                          configManager.data.manualYear,
                          configManager.data.manualMonth,
                          configManager.data.manualDay,
                          configManager.data.manualHour,
                          configManager.data.manualMinute);
        }
        fetchWeather();
    } else {
        Serial.println(F("[Boot] Recovery mode — NTP and weather skipped"));
    }

    lastUserActivity   = millis();
    lastNtpSync        = millis();
    lastWeatherSync    = millis();
    lastDisplayRefresh = 0;          // force first render immediately
    lastHeartbeat      = millis();

    digitalWrite(STATUS_LED_PIN, HIGH); // LED off after boot
    Serial.printf("[Boot] complete — heap=%u B\n", ESP.getFreeHeap());
    Serial.println(F("================================"));
}

// ─────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────
void loop() {
    // Framework services
    WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();

    unsigned long now = millis();

    // ── Button handling ──────────────────────────────────────────────
    button.update();

    // Single-click / double-click via event queue
    {
        ButtonEvent evt = button.getEvent();
        if (evt == BE_CLICK) {
            lastUserActivity = now;
            if (!inRecovery) {
                uint8_t brtIdx = (configManager.data.brightness + 1) % 3;
                configManager.data.brightness = brtIdx;
                configManager.save();
                FastLED.setBrightness(BRT_LEVELS[brtIdx]);
                Serial.printf("[Button] Click — brightness level %d (raw=%d)\n",
                              brtIdx, BRT_LEVELS[brtIdx]);
            }
        } else if (evt == BE_DOUBLE_CLICK) {
            lastUserActivity = now;
            if (!inRecovery) {
                Serial.println(F("[Button] Double-click — force NTP+weather refresh"));
                if (timeSync.waitForSyncResult(5000) == 0) {
                    ntpSynced = true;
                    Serial.println(F("[NTP] re-synced OK"));
                } else {
                    Serial.println(F("[NTP] re-sync timeout"));
                }
                fetchWeather();
                lastNtpSync        = now;
                lastWeatherSync    = now;
                lastDisplayRefresh = 0;
            }
        }
    }

    // Long-press (1 s) → recovery;  very-long-press (5 s) → reboot
    // (Polled directly to avoid ButtonHandler static-variable bug)
    if (button.isPressed()) {
        lastUserActivity = now;
        uint32_t dur = button.getPressDuration();

        if (!veryLongHandled && dur >= 5000UL) {
            veryLongHandled = true;
            Serial.println(F("[Button] 5 s hold — rebooting"));
            delay(200);
            ESP.restart();
        } else if (!inRecovery && !longPressHandled && dur >= LONG_PRESS_MS) {
            longPressHandled = true;
            Serial.println(F("[Button] 1 s hold — entering recovery mode"));
            RTCData rtc = { RTC_MAGIC, 1 };
            ESP.rtcUserMemoryWrite(0,
                reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
            delay(50);
            ESP.restart();
        }
    } else {
        longPressHandled = false;
        veryLongHandled  = false;
    }

    // Active WebSocket client counts as user interaction
    if (GUI.ws.count() > 0) lastUserActivity = now;

    bool isIdle = (now - lastUserActivity > IDLE_TIMEOUT_MS);

    // ── Display refresh ──────────────────────────────────────────────
    if (inRecovery) {
        drawScrollText("RECOVERY", C_ORANGE);
        flushDisplay();
        delay(1);   // yield to TCP/IP stack
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

    // ── Dashboard WebSocket push ─────────────────────────────────────
    if (now - lastDashUpdate >= (unsigned long)DASH_INT_MS) {
        lastDashUpdate = now;
        updateDashboard();
    }

    // ── Periodic NTP re-sync ─────────────────────────────────────────
    if (!inRecovery && now - lastNtpSync >= NTP_INTERVAL_MS) {
        lastNtpSync = now;
        Serial.println(F("[NTP] periodic re-sync..."));
        if (timeSync.waitForSyncResult(5000) == 0) {
            ntpSynced = true;
            Serial.println(F("[NTP] OK"));
        } else {
            Serial.println(F("[NTP] timeout — keeping previous time"));
        }
    }

    // ── Periodic weather re-fetch ────────────────────────────────────
    if (!inRecovery && now - lastWeatherSync >= WEATHER_INT_MS) {
        lastWeatherSync = now;
        fetchWeather();
    }

    // ── Heartbeat serial output every 60 s ──────────────────────────
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        lastHeartbeat = now;
        struct tm* t = getTime();
        if (t) {
            Serial.printf("[Heartbeat] %02d:%02d:%02d | heap=%uB | ntp=%d | "
                          "wx=%d %.1fC | orient=%d | idle=%d\n",
                          t->tm_hour, t->tm_min, t->tm_sec,
                          ESP.getFreeHeap(),
                          ntpSynced,
                          weatherCode, weatherTemp,
                          configManager.data.displayOrientation,
                          isIdle);
        }
    }

    // ── Status LED ───────────────────────────────────────────────────
    updateStatusLed();

    yield();
}
