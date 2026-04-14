// DisplayManager.cpp
#include "DisplayManager.h"

const uint8_t DisplayManager::BRIGHTNESS_TABLE[3] = {50, 128, 255};

DisplayManager::DisplayManager()
    : _recovery(false),
      _scrollOffset(MATRIX_WIDTH),
      _lastScroll(0) {
    memset(_matrix, 0, sizeof(_matrix));
    memset(_leds,   0, sizeof(_leds));
}

// ── Public ──────────────────────────────────────────────────────────────────

void DisplayManager::begin() {
    FastLED.addLeds<WS2812B, LED_MATRIX_PIN, GRB>(_leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS_TABLE[1]); // medium default
    FastLED.clear(true);
}

void DisplayManager::setBrightness(uint8_t level) {
    level = constrain(level, 0, 2);
    FastLED.setBrightness(BRIGHTNESS_TABLE[level]);
    FastLED.show();
}

void DisplayManager::render(int hour, int minute,
                             int weekday, int day,
                             uint32_t weatherColor) {
    _recovery = false;
    _clearMatrix();

    const CRGB WHITE(0xFF, 0xFF, 0xFF);
    ClockFace::render(_matrix, hour, minute, WHITE, WHITE);
    InfoColumns::render(_matrix, day, weekday, weatherColor);

    _push();
}

void DisplayManager::showRecovery() {
    _recovery      = true;
    _scrollOffset  = MATRIX_WIDTH;   // start off-screen right
    _lastScroll    = 0;
    _clearMatrix();
    _push();
}

void DisplayManager::scrollTick() {
    if (!_recovery) return;

    unsigned long now = millis();
    if (now - _lastScroll < 50) return; // 50 ms per column = ~20 cols/s
    _lastScroll = now;

    _clearMatrix();
    ClockFace::renderScroll(_matrix, _scrollOffset, CRGB(0xFF, 0x66, 0x00)); // orange
    _push();

    _scrollOffset--;

    // Wrap: restart once the full text has scrolled past the left edge
    if (_scrollOffset < -ClockFace::scrollTextWidth()) {
        _scrollOffset = MATRIX_WIDTH;
    }
}

void DisplayManager::clear() {
    _clearMatrix();
    _push();
}

// ── Private ─────────────────────────────────────────────────────────────────

void DisplayManager::_clearMatrix() {
    for (int r = 0; r < MATRIX_HEIGHT; r++)
        for (int c = 0; c < MATRIX_WIDTH; c++)
            _matrix[r][c] = CRGB::Black;
}

void DisplayManager::_push() {
    // Apply the column-snake physical mapping (mirrors LEDMatrixLayout.h logic)
    // Even columns: bottom→top  (physicalRow = HEIGHT-1-row)
    // Odd  columns: top→bottom  (physicalRow = row)
    for (uint8_t col = 0; col < MATRIX_WIDTH; col++) {
        for (uint8_t row = 0; row < MATRIX_HEIGHT; row++) {
            uint8_t physRow = (col % 2 == 0)
                              ? (MATRIX_HEIGHT - 1 - row)
                              : row;
            uint16_t idx = col * MATRIX_HEIGHT + physRow;
            if (idx < NUM_LEDS) {
                _leds[idx] = _matrix[row][col];
            }
        }
    }
    FastLED.show();
}
