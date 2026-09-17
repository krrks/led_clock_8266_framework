// ClockDisplay.cpp — display primitives and clock face renderers
// Uses NeoPixelBus DMA (GPIO3/RX) for WiFi-safe WS2812B output.
#include "ClockDisplay.h"
#include "AppState.h"
#include "NeoStrip.h"
#include "FontData.h"
#include "PinDefinitions.h"

#include <ESP8266WiFi.h>
#include "wifi/WiFiService.h"
#include "config/ConfigManager.h"

// ─── Colour constants ────────────────────────────────────────────────────
const uint32_t C_WHITE  = 0xFFFFFF;
const uint32_t C_ORANGE = 0xFF6600;
const uint32_t C_CYAN   = 0x00CCCC;
const uint32_t C_GREEN  = 0x00CC00;
const uint32_t C_YELLOW = 0xDDCC00;

uint32_t mkRgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// Weekday-group colour for the day indicator (row 7, clock face)
static uint32_t weekdayColor(int wday) {
    if (wday == 1 || wday == 2) return mkRgb(0x00, 0xCC, 0x00); // Mon/Tue — green
    if (wday == 3 || wday == 4) return mkRgb(0x00, 0x66, 0xFF); // Wed/Thu — blue
    return mkRgb(0xFF, 0x00, 0x00);                             // Fri/Sat/Sun — red
}

// 6×7 weather glyphs, row-major, bit5 = leftmost column
static const uint8_t WX_ICONS[9][7] = {
    {0x12, 0x0C, 0x1E, 0x1E, 0x0C, 0x00, 0x00},  // 0 sun (clear)
    {0x00, 0x00, 0x0C, 0x1E, 0x00, 0x00, 0x00},  // 1 few clouds
    {0x00, 0x0C, 0x1E, 0x3F, 0x00, 0x00, 0x00},  // 2 scattered clouds
    {0x0C, 0x1E, 0x3F, 0x3F, 0x3F, 0x00, 0x00},  // 3 broken / overcast
    {0x00, 0x0C, 0x1E, 0x00, 0x14, 0x14, 0x14},  // 4 shower / drizzle
    {0x00, 0x0C, 0x1E, 0x00, 0x2A, 0x2A, 0x2A},  // 5 rain
    {0x00, 0x0C, 0x1E, 0x00, 0x06, 0x0C, 0x18},  // 6 thunderstorm
    {0x00, 0x0C, 0x1E, 0x00, 0x14, 0x0A, 0x14},  // 7 snow
    {0x00, 0x1E, 0x00, 0x3F, 0x00, 0x1E, 0x00},  // 8 mist / fog
};

// OWM condition code → glyph index (nullptr = no data, icon off)
static const uint8_t* wxIconGlyph(int16_t code) {
    if (code == 800)                 return WX_ICONS[0];
    if (code == 801)                 return WX_ICONS[1];
    if (code == 802)                 return WX_ICONS[2];
    if (code >= 803 && code <= 804)  return WX_ICONS[3];
    if (code >= 300 && code <= 321)  return WX_ICONS[4];
    if (code >= 500 && code <= 531)  return WX_ICONS[5];
    if (code >= 200 && code <= 232)  return WX_ICONS[6];
    if (code >= 600 && code <= 622)  return WX_ICONS[7];
    if (code >= 700 && code <= 781)  return WX_ICONS[8];
    return nullptr;
}

// ─── Brightness ───────────────────────────────────────────────────────────
// Read from configManager at render time so changes take effect immediately.
static uint8_t getCurrentBrightness() {
    auto& d = configManager.data;
    if (d.brightness == 0) return d.brightDim;
    if (d.brightness == 2) return d.brightBrt;
    return d.brightMed;
}

// ─── Current time helper ──────────────────────────────────────────────────
static bool getCurrentTime(struct tm& t) {
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

// ─── Orientation: rotation + flip applied independently ──────────────────
// curRotation: 0=0°  1=90°CW  2=180°  3=270°CW
// curFlip:     0=none  1=H-flip  2=V-flip
static void applyOrientation(uint32_t src[MATRIX_HEIGHT][MATRIX_WIDTH],
                              uint32_t dst[MATRIX_HEIGHT][MATRIX_WIDTH]) {
    for (int r = 0; r < MATRIX_HEIGHT; r++) {
        for (int c = 0; c < MATRIX_WIDTH; c++) {
            int sr, sc;
            switch (curRotation) {
                case 1:  // 90° CW
                    sr = constrain((MATRIX_WIDTH - 1 - c) * MATRIX_HEIGHT / MATRIX_WIDTH,
                                   0, MATRIX_HEIGHT - 1);
                    sc = constrain(r * MATRIX_WIDTH / MATRIX_HEIGHT,
                                   0, MATRIX_WIDTH - 1);
                    break;
                case 2:  // 180°
                    sr = MATRIX_HEIGHT - 1 - r;
                    sc = MATRIX_WIDTH  - 1 - c;
                    break;
                case 3:  // 270° CW
                    sr = constrain(c * MATRIX_HEIGHT / MATRIX_WIDTH,
                                   0, MATRIX_HEIGHT - 1);
                    sc = constrain((MATRIX_HEIGHT - 1 - r) * MATRIX_WIDTH / MATRIX_HEIGHT,
                                   0, MATRIX_WIDTH - 1);
                    break;
                default: // 0° — no rotation
                    sr = r; sc = c;
                    break;
            }
            // Hardcoded vertical mirror (top↔bottom) — panel is mounted
            // upside-down. configManager.data.flip is ignored.
            sr = MATRIX_HEIGHT - 1 - sr;
            dst[r][c] = src[sr][sc];
        }
    }
}

// ─── Primitives ──────────────────────────────────────────────────────────
void clearDisplay() {
    memset(displayMatrix, 0, sizeof(displayMatrix));
}

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

int strPxW(const char* s) {
    int w = 0;
    for (int i = 0; s[i]; i++) w += (int)getCharWidth(s[i]) + 1;
    return (w > 0) ? w - 1 : 0;
}

int drawStr(const char* s, int x, uint32_t color) {
    for (int i = 0; s[i]; i++) x += drawChar(s[i], x, color) + 1;
    return x;
}

void drawScroll(const char* txt, uint32_t color) {
    int txtW   = strPxW(txt);
    int totalW = MATRIX_WIDTH + txtW + 8;
    int x      = MATRIX_WIDTH - scrollOff;
    for (int i = 0; txt[i]; i++) x += drawChar(txt[i], x, color) + 1;
    if (++scrollOff >= totalW) scrollOff = 0;
}

// ─── flushDisplay — orientation → snake-map → brightness scale → DMA ─────
// NeoPixelBus Show() is DMA-driven; it returns immediately and does NOT
// block WiFi background tasks. No interrupt lock needed.
void flushDisplay() {
    uint32_t oriented[MATRIX_HEIGHT][MATRIX_WIDTH];
    applyOrientation(displayMatrix, oriented);

    uint32_t buf[NUM_LEDS];
    convertToSnakeOrder(oriented, buf);

    uint8_t bri = getCurrentBrightness();

    for (int i = 0; i < NUM_LEDS; i++) {
        // +127 rounds instead of flooring: at very low brightness (dim=1)
        // plain truncation turns 0xCC..0xDD components into 0, hiding
        // non-white faces (date/temp/IP) entirely.
        uint8_t r = (uint8_t)((((buf[i] >> 16) & 0xFF) * (uint16_t)bri + 127u) / 255u);
        uint8_t g = (uint8_t)((((buf[i] >>  8) & 0xFF) * (uint16_t)bri + 127u) / 255u);
        uint8_t b = (uint8_t)((( buf[i]         & 0xFF) * (uint16_t)bri + 127u) / 255u);
        neoStrip.SetPixelColor(i, RgbColor(r, g, b));
    }
    neoStrip.Show();
}

// ─── Faces ───────────────────────────────────────────────────────────────

void drawClockFace() {
    struct tm t = {};
    if (!getCurrentTime(t)) return;

    int hr = configManager.data.use24h ? t.tm_hour : (t.tm_hour % 12 ? t.tm_hour % 12 : 12);
    char d[5]; snprintf(d, sizeof(d), "%02d%02d", hr, t.tm_min);

    int x = 0;
    x += drawChar(d[0], x, mainColor) + 1;
    x += drawChar(d[1], x, mainColor) + 1;
    x += drawChar(':',  x, mainColor) + 1;
    x += drawChar(d[2], x, mainColor) + 1;
         drawChar(d[3], x, mainColor);

    // ── Weather icon: cols 26-31 (6×7), monochrome theme colour ──────────
    if (weatherEnabled && weatherCode != 0) {
        const uint8_t* icon = wxIconGlyph(weatherCode);
        if (icon) {
            for (uint8_t r = 0; r < 7; r++)
                for (uint8_t c = 0; c < 6; c++)
                    if (icon[r] & (1 << (5 - c)))
                        displayMatrix[r][26 + c] = mainColor;
        }
    }

    // ── Day indicator: row 7, column = day of month, colour = weekday ────
    if (t.tm_mday >= 1 && t.tm_mday <= 31)
        displayMatrix[7][t.tm_mday - 1] = weekdayColor(t.tm_wday);
}

void drawDateFace() {
    struct tm t = {};
    char s[12];
    if (getCurrentTime(t)) snprintf(s, sizeof(s), "%02d-%02d", t.tm_mon + 1, t.tm_mday);
    else                    strlcpy(s, "NODATE", sizeof(s));
    int w = strPxW(s);
    drawStr(s, max(0, (MATRIX_WIDTH - w) / 2), mainColor);
}

void drawTempFace() {
    char s[12];
    if (weatherEnabled && weatherCode != 0)
        snprintf(s, sizeof(s), "%.1fC", weatherTemp);
    else
        strlcpy(s, "WX OFF", sizeof(s));
    int w = strPxW(s);
    drawStr(s, max(0, (MATRIX_WIDTH - w) / 2), mainColor);
}

// ── IP paged display state ────────────────────────────────────────────────
static char          ipBuf[40]  = "NO WIFI";
static char          pages[4][20];
static uint8_t       nPages     = 1;
static uint8_t       page       = 0;
static unsigned long tPage      = 0;

static void buildIpPages() {
    // Width-fit paging: 5px digits mean "192.168" (37px) exceeds the
    // 32px matrix, so pack octets into pages that fit, e.g.
    // "192." / "168." / "3.47"
    nPages = 0;
    uint8_t cur = 0;
    pages[0][0] = 0;
    const char* p = ipBuf;
    while (*p && nPages < 4) {
        char tok[12]; size_t ti = 0;
        while (*p && *p != '.' && *p != ' ') tok[ti++] = *p++;
        if (*p == '.') tok[ti++] = *p++;
        while (*p == ' ') p++;  // drop spaces between tokens
        tok[ti] = 0;

        char tmp[24];
        strlcpy(tmp, pages[cur], sizeof(tmp));
        strlcat(tmp, tok, sizeof(tmp));
        if (pages[cur][0] == 0 || strPxW(tmp) <= MATRIX_WIDTH - 2) {
            strlcpy(pages[cur], tmp, sizeof(pages[cur]));
        } else {
            cur = ++nPages;
            strlcpy(pages[cur], tok, sizeof(pages[cur]));
        }
    }
    nPages = cur + 1;
    page = 0; tPage = millis();
}

void resetIpPages() {
    if (WiFi.status() == WL_CONNECTED)
        snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.localIP().toString().c_str());
    else
        strlcpy(ipBuf, "NO WIFI", sizeof(ipBuf));
    buildIpPages();
}

unsigned long ipFaceTimeoutMs() {
    return (unsigned long)nPages * PAGE_IP_MS;
}

void drawIPFace() {
    // Pages are (re)built on face entry via resetIpPages(); no periodic
    // refresh here — it would reset the page counter mid-cycle.
    if (millis() - tPage >= PAGE_IP_MS) { tPage = millis(); page = (page + 1) % nPages; }
    const char* s = pages[page % nPages];

    int w = strPxW(s);
    drawStr(s, max(0, (MATRIX_WIDTH - w) / 2), mainColor);
}
