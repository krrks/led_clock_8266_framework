// InfoColumns.h
// Renders the three single-column pixel bars on the right side of the matrix.
//
// Column map (cols 25-31):
//   [25]   gap
//   [26]   DATE column     – 1 pixel per week of month (1-5), white
//   [27]   gap
//   [28]   WEEKDAY column  – 1-7 pixels (Mon-Sun), cyan / orange on weekends
//   [29]   gap
//   [30]   WEATHER column  – always 4 pixels, colour = OWM condition
//   [31]   trailing pad
//
// All bars are lit bottom-to-top (d_up convention).

#ifndef INFOCOLUMNS_H
#define INFOCOLUMNS_H

#include <Arduino.h>
#include <FastLED.h>
#include "LEDMatrixLayout.h"

class InfoColumns {
public:
    // day     : day of month (1-31)
    // weekday : 1=Mon .. 7=Sun
    // weatherColorPacked : 0x00RRGGBB from WeatherService::getConditionColor()
    static void render(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                       int day, int weekday,
                       uint32_t weatherColorPacked);

private:
    // Light 'pixels' LEDs from the bottom of column 'col'
    static void _bar(CRGB matrix[MATRIX_HEIGHT][MATRIX_WIDTH],
                     int col, int pixels, CRGB color);

    static CRGB _unpack(uint32_t rgb);
};

#endif // INFOCOLUMNS_H
