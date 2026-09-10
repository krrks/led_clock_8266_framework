// InfoColumns.cpp
#include "InfoColumns.h"

// Physical column positions
static const int COL_DATE    = 26;
static const int COL_WEEKDAY = 28;
static const int COL_WEATHER = 30;

// ── Public ──────────────────────────────────────────────────────────────────

void InfoColumns::render(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                          int day, int weekday, uint32_t weatherColorPacked) {

    // ── DATE column ──────────────────────────────────────────────────────────
    // 1 pixel per 7-day week: days 1-7 → 1 px, 8-14 → 2 px, … 29-31 → 5 px
    int datePixels = constrain(((day - 1) / 7) + 1, 1, 5);
    _bar(matrix, COL_DATE, datePixels, CRGB(0xFF, 0xFF, 0xFF)); // white

    // ── WEEKDAY column ───────────────────────────────────────────────────────
    // Mon=1 px … Sun=7 px; weekend (Sat/Sun) in orange, weekday in cyan
    weekday = constrain(weekday, 1, 7);
    CRGB wdColor = (weekday >= 6)
                   ? CRGB(0xFF, 0x66, 0x00)   // orange – weekend
                   : CRGB(0x00, 0xAA, 0xAA);  // cyan   – weekday
    _bar(matrix, COL_WEEKDAY, weekday, wdColor);

    // ── WEATHER column ───────────────────────────────────────────────────────
    // Fixed 4 pixels; colour encodes weather condition
    _bar(matrix, COL_WEATHER, 4, _unpack(weatherColorPacked));
}

// ── Private ─────────────────────────────────────────────────────────────────

void InfoColumns::_bar(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                        int col, int pixels, CRGB color) {
    if (col < 0 || col >= MATRIX_WIDTH) return;
    pixels = constrain(pixels, 0, MATRIX_HEIGHT);
    for (int i = 0; i < pixels; i++) {
        // i=0 → bottom row (MATRIX_HEIGHT-1), i=1 → one above, etc.
        matrix[MATRIX_HEIGHT - 1 - i][col] = color;
    }
}

CRGB InfoColumns::_unpack(uint32_t rgb) {
    return CRGB(
        (rgb >> 16) & 0xFF,
        (rgb >>  8) & 0xFF,
         rgb        & 0xFF
    );
}
