// ClockFace.cpp
#include "ClockFace.h"

// Column positions for each time element
static const int COL_H_TENS  =  0;
static const int COL_H_UNITS =  6;
static const int COL_COLON   = 12;
static const int COL_M_TENS  = 14;
static const int COL_M_UNITS = 20;

// Scrolling recovery text; leading/trailing spaces provide visual gap
static const char SCROLL_TEXT[] = "  RECOVERY  ";
static const int  CHAR_STRIDE   = 6; // 5 pixels wide + 1 gap

// ── Public ──────────────────────────────────────────────────────────────────

void ClockFace::render(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                       int hour, int minute,
                       CRGB digitColor, CRGB colonColor) {
    char buf[5];
    snprintf(buf, sizeof(buf), "%02d%02d", hour, minute);

    _renderChar(matrix, COL_H_TENS,  buf[0], digitColor);
    _renderChar(matrix, COL_H_UNITS, buf[1], digitColor);
    _renderChar(matrix, COL_COLON,   ':',    colonColor);
    _renderChar(matrix, COL_M_TENS,  buf[2], digitColor);
    _renderChar(matrix, COL_M_UNITS, buf[3], digitColor);
}

void ClockFace::renderScroll(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                              int offset, CRGB color) {
    int len = strlen(SCROLL_TEXT);
    for (int i = 0; i < len; i++) {
        _renderChar(matrix, offset + i * CHAR_STRIDE, SCROLL_TEXT[i], color);
    }
}

int ClockFace::scrollTextWidth() {
    return (int)strlen(SCROLL_TEXT) * CHAR_STRIDE;
}

// ── Private ─────────────────────────────────────────────────────────────────

void ClockFace::_renderChar(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                              int startCol, char c, CRGB color) {
    const uint8_t *fontData = getCharFontData(c);
    if (!fontData) return;

    uint8_t width = getCharWidth(c);

    for (int col = 0; col < (int)width; col++) {
        int matCol = startCol + col;
        // Skip columns outside the physical matrix
        if (matCol < 0 || matCol >= MATRIX_WIDTH) continue;

        uint8_t colData = fontData[col];
        // Bit N of colData = row N (bit 0 = top row 0)
        for (int row = 0; row < DIGIT_HEIGHT; row++) {
            if (colData & (1 << row)) {
                matrix[row][matCol] = color;
            }
        }
    }
}
