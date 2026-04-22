// ClockDisplay.cpp — display primitives and clock face renderers
#include "ClockDisplay.h"
#include "AppState.h"
#include "FontData.h"
#include "PinDefinitions.h"

#include <FastLED.h>
#include <ESP8266WiFi.h>
#include "WiFiManager.h"
#include "configManager.h"

// ─── Colour constants ────────────────────────────────────────────────────
const uint32_t C_WHITE  = 0xFFFFFF;
const uint32_t C_ORANGE = 0xFF6600;
const uint32_t C_CYAN   = 0x00CCCC;
const uint32_t C_GREEN  = 0x00CC00;
const uint32_t C_YELLOW = 0xDDCC00;

uint32_t mkRgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// OWM condition code → bar colour.
// Groups per openweathermap.org/weather-conditions:
//   2xx Thunderstorm | 3xx Drizzle | 5xx Rain | 6xx Snow
//   7xx Atmosphere (fog/mist/haze/dust/…)
//   800 Clear | 801-802 Few/Scattered clouds | 803-804 Broken/Overcast
uint32_t wxColor(int16_t code) {
    if (code == 0)                  return 0x444444;            // no data: dim grey
    if (code >= 200 && code < 300)  return mkRgb(160,  0, 200); // Thunderstorm: purple
    if (code >= 300 && code < 400)  return mkRgb( 80,140, 255); // Drizzle: light blue
    if (code >= 500 && code < 600)  return mkRgb(  0, 60, 255); // Rain: blue
    if (code >= 600 && code < 700)  return mkRgb(160,200, 255); // Snow: icy blue
    if (code >= 700 && code < 800)  return mkRgb(120,120, 120); // Atmosphere: grey
    if (code == 800)                return mkRgb(255,200,   0); // Clear: yellow
    if (code >= 801 && code <= 802) return mkRgb(180,180,  60); // Few/scattered: pale yellow
    if (code >= 803 && code <= 804) return mkRgb(140,140, 140); // Broken/overcast: grey
    return C_WHITE;
}

// ─── Internal: current time from NTP or manual drift ─────────────────────
static bool getCurrentTime(struct tm &t) {
    if (ntpSynced) {
        time_t now = time(nullptr);
        struct tm* p = localtime(&now);
        if (p) { t = *p; return true; }
    }
    if (manualBase > 0) {
        time_t ts = mktime(&manualTm) + (time_t)((millis() - manualBase) / 1000UL);
        struct tm* p = localtime(&ts);
        if (p) { t = *p; return true; }
    }
    return false;
}

// ─── Internal: orientation remapping ─────────────────────────────────────
static void applyOrientation(uint32_t src[MATRIX_HEIGHT][MATRIX_WIDTH],
                              uint32_t dst[MATRIX_HEIGHT][MATRIX_WIDTH]) {
    for (int r = 0; r < MATRIX_HEIGHT; r++)
        for (int c = 0; c < MATRIX_WIDTH; c++) {
            uint32_t px;
            switch (curOrientation) {
                case 2:  px = src[MATRIX_HEIGHT-1-r][MATRIX_WIDTH-1-c]; break;
                case 4:  px = src[r][MATRIX_WIDTH-1-c];                 break;
                case 5:  px = src[MATRIX_HEIGHT-1-r][c];                break;
                default: px = src[r][c];                                break;
            }
            dst[r][c] = px;
        }
}

// ─── Primitives ──────────────────────────────────────────────────────────
void clearDisplay() {
    memset(displayMatrix, 0, sizeof(displayMatrix));
}

// Draws one character at column x.  Returns its pixel width (caller adds +1 gap).
int drawChar(char c, int x, uint32_t color) {
    const uint8_t* fd = getCharFontData(c);
    uint8_t w = getCharWidth(c);
    if (!fd) return (int)w;
    for (uint8_t col = 0; col < w; col++) {
        int px = x + (int)col;
        if (px < 0 || px >= MATRIX_WIDTH) continue;
        for (uint8_t row = 0; row < DIGIT_HEIGHT; row++)
            if ((fd[col] >> row) & 1)
                displayMatrix[row][px] = color;
    }
    return (int)w;
}

// Total pixel width of a string (no trailing gap on the last character).
int strPxW(const char* s) {
    int w = 0;
    for (int i = 0; s[i]; i++) w += (int)getCharWidth(s[i]) + 1;
    return (w > 0) ? w - 1 : 0;
}

// Draws string at x; returns x after last char + gap.
int drawStr(const char* s, int x, uint32_t color) {
    for (int i = 0; s[i]; i++) x += drawChar(s[i], x, color) + 1;
    return x;
}

// Scrolling text — advances the global scrollOff by 1 each call.
// Caller must call clearDisplay() first and respect SCROLL_FRAME_MS rate.
void drawScroll(const char* txt, uint32_t color) {
    int txtW   = strPxW(txt);
    int totalW = MATRIX_WIDTH + txtW + 8;   // 8-px pause between repeats
    int x      = MATRIX_WIDTH - scrollOff;
    for (int i = 0; txt[i]; i++) x += drawChar(txt[i], x, color) + 1;
    if (++scrollOff >= totalW) scrollOff = 0;
}

// Push displayMatrix → physical LED strip (orientation + snake-map).
void flushDisplay() {
    uint32_t oriented[MATRIX_HEIGHT][MATRIX_WIDTH];
    applyOrientation(displayMatrix, oriented);
    uint32_t buf[NUM_LEDS];
    convertToSnakeOrder(oriented, buf);
    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i].r = (uint8_t)((buf[i] >> 16) & 0xFF);
        leds[i].g = (uint8_t)((buf[i] >>  8) & 0xFF);
        leds[i].b = (uint8_t)( buf[i]         & 0xFF);
    }
    FastLED.show();
}

// ─── Faces ───────────────────────────────────────────────────────────────

// CLOCK: HH:MM digits + 3 info bar columns
void drawClockFace() {
    struct tm t = {};
    if (!getCurrentTime(t)) return;

    int hr = configManager.data.use24h ? t.tm_hour : (t.tm_hour % 12 ?: 12);
    char d[5]; snprintf(d, sizeof(d), "%02d%02d", hr, t.tm_min);

    // HH:MM block — cols 0-24
    int x = 0;
    x += drawChar(d[0], x, C_WHITE) + 1;
    x += drawChar(d[1], x, C_WHITE) + 1;
    x += drawChar(':',  x, C_WHITE) + 1;
    x += drawChar(d[2], x, C_WHITE) + 1;
         drawChar(d[3], x, C_WHITE);

    // Info bars (bottom-up; row index MATRIX_HEIGHT-1 is the bottom row)
    int mday = t.tm_mday;
    int wday = (t.tm_wday == 0) ? 7 : t.tm_wday;  // 0=Sun→7, 1=Mon..6=Sat

    // Col 26: DATE bar — 1 px per 7-day week of month (1-5 px), white
    int wom = constrain((mday - 1) / 7 + 1, 1, 5);
    for (int i = 0; i < wom; i++)
        displayMatrix[MATRIX_HEIGHT - 1 - i][26] = C_WHITE;

    // Col 28: WEEKDAY bar — Mon=1..Sun=7 px; orange weekend, cyan weekday
    uint32_t wdCol = (wday >= 6) ? C_ORANGE : C_CYAN;
    for (int i = 0; i < wday; i++)
        displayMatrix[MATRIX_HEIGHT - 1 - i][28] = wdCol;

    // Col 30: WEATHER bar — 4 px, colour = OWM condition group
    if (weatherEnabled && weatherCode != 0) {
        uint32_t wxc = wxColor(weatherCode);
        for (int i = 0; i < 4; i++)
            displayMatrix[MATRIX_HEIGHT - 1 - i][30] = wxc;
    }
}

// DATE: static "22APR" centred
void drawDateFace() {
    static const char* MON[] = {
        "JAN","FEB","MAR","APR","MAY","JUN",
        "JUL","AUG","SEP","OCT","NOV","DEC"
    };
    struct tm t = {};
    char s[12];
    if (getCurrentTime(t)) snprintf(s, sizeof(s), "%02d%s", t.tm_mday, MON[t.tm_mon]);
    else                    strlcpy(s, "NODATE", sizeof(s));
    int w = strPxW(s);
    drawStr(s, max(0, (MATRIX_WIDTH - w) / 2), C_GREEN);
}

// TEMP: static temperature or "WX OFF"
void drawTempFace() {
    char s[12];
    if (weatherEnabled && weatherCode != 0)
        snprintf(s, sizeof(s), "%.1fC", weatherTemp);
    else
        strlcpy(s, "WX OFF", sizeof(s));
    int w = strPxW(s);
    drawStr(s, max(0, (MATRIX_WIDTH - w) / 2), C_YELLOW);
}

// IP: scrolling WiFi IP address
void drawIPFace() {
    static char          ipBuf[40]  = "NO WIFI";
    static unsigned long tIpCache   = 0;
    if (millis() - tIpCache > 5000) {
        tIpCache = millis();
        if (WiFi.status() == WL_CONNECTED)
            snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.localIP().toString().c_str());
        else
            strlcpy(ipBuf, "NO WIFI", sizeof(ipBuf));
    }
    drawScroll(ipBuf, C_CYAN);
}
