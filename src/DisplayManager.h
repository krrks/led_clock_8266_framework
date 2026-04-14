// DisplayManager.h
// Owns the FastLED strip and the logical CRGB display matrix.
// Applies the column-snake physical mapping from LEDMatrixLayout.h.
//
// Two operating faces:
//   render()      – normal clock face (digits + info columns)
//   showRecovery() + scrollTick() – orange scrolling "RECOVERY" text

#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include "PinDefinitions.h"
#include "LEDMatrixLayout.h"
#include "ClockFace.h"
#include "InfoColumns.h"

class DisplayManager {
public:
    DisplayManager();

    // Call once in setup() – initialises FastLED
    void begin();

    // level: 0 = dim (50), 1 = medium (128), 2 = full (255)
    void setBrightness(uint8_t level);

    // Full clock face render – called every 30 s in normal mode
    void render(int hour, int minute,
                int weekday, int day,
                uint32_t weatherColor);

    // Enter recovery face (starts the scroll loop)
    void showRecovery();

    // Advance the scroll by one column; call every loop() in recovery mode.
    // Internally rate-limited to ~50 ms per column shift.
    void scrollTick();

    // Blank all LEDs
    void clear();

private:
    CRGB _matrix[MATRIX_HEIGHT][MATRIX_WIDTH]; // logical display
    CRGB _leds[NUM_LEDS];                       // physical LED strip

    bool          _recovery;
    int           _scrollOffset;
    unsigned long _lastScroll;

    static const uint8_t BRIGHTNESS_TABLE[3];

    void _clearMatrix();
    void _push();   // snake-map matrix → leds, then FastLED.show()
};

#endif // DISPLAYMANAGER_H
