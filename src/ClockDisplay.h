#pragma once
// ClockDisplay.h — display primitives and face renderers
//
// Primitives: clearDisplay, drawChar, strPxW, drawStr, drawScroll, flushDisplay
// Faces:      drawClockFace, drawDateFace, drawTempFace, drawIPFace

#include <Arduino.h>
#include <stdint.h>

// Colour helpers
uint32_t mkRgb(uint8_t r, uint8_t g, uint8_t b);
uint32_t wxColor(int16_t code);

// OWM condition code → colour constants (packed 0x00RRGGBB)
extern const uint32_t C_WHITE;
extern const uint32_t C_ORANGE;
extern const uint32_t C_CYAN;
extern const uint32_t C_GREEN;
extern const uint32_t C_YELLOW;

// Primitives
void clearDisplay();
int  drawChar(char c, int x, uint32_t color);
int  strPxW(const char* s);
int  drawStr(const char* s, int x, uint32_t color);
void drawScroll(const char* txt, uint32_t color);   // advances global scrollOff
void flushDisplay();                                  // orientation + snake-map → FastLED.show()

// Faces
void drawClockFace();
void drawDateFace();
void drawTempFace();
void drawIPFace();
