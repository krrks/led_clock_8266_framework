// ClockFace.h
// Renders HH:MM digits into the CRGB display matrix and provides the
// scrolling "RECOVERY" text for recovery mode.
//
// Time block column layout (25 columns, 0-24):
//   [0-4]  H tens digit    (5 cols)
//   [5]    space
//   [6-10] H units digit   (5 cols)
//   [11]   space
//   [12]   colon           (1 col, PUNCT_COLON  0x14)
//   [13]   space
//   [14-18] M tens digit   (5 cols)
//   [19]   space
//   [20-24] M units digit  (5 cols)
//
// Digits use rows 0-6 (DIGIT_HEIGHT = 7). Row 7 is always dark.
// Bit 0 of each font byte = row 0 (top).

#ifndef CLOCKFACE_H
#define CLOCKFACE_H

#include <Arduino.h>
#include <FastLED.h>
#include "LEDMatrixLayout.h"
#include "FontData.h"

class ClockFace {
public:
    // Render HH:MM into the matrix
    static void render(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                       int hour, int minute,
                       CRGB digitColor, CRGB colonColor);

    // Render one frame of the scrolling RECOVERY text.
    // offset: starting column for the first character (decrements each tick).
    static void renderScroll(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                              int offset, CRGB color);

    // Total pixel width of the scroll text (chars × 6), used to wrap offset
    static int scrollTextWidth();

private:
    static void _renderChar(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                             int startCol, char c, CRGB color);
};

#endif // CLOCKFACE_H
