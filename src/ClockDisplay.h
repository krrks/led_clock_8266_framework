#pragma once
// ClockDisplay.h — display primitives and face renderers
//
// Colours are packed uint32_t (0x00RRGGBB).
// flushDisplay() applies orientation, snake-maps, scales brightness,
// then calls neoStrip.Show() via NeoPixelBus (WiFi-safe DMA on GPIO3).

#include <Arduino.h>
#include <stdint.h>

// Colour helpers
uint32_t mkRgb(uint8_t r, uint8_t g, uint8_t b);

// Shared colour constants (defined in ClockDisplay.cpp)
extern const uint32_t C_WHITE;
extern const uint32_t C_ORANGE;
extern const uint32_t C_CYAN;
extern const uint32_t C_GREEN;
extern const uint32_t C_YELLOW;

// Primitives
void clearDisplay();
int  drawChar(char c, int x, uint32_t color);  // returns char pixel width
int  strPxW(const char* s);                     // total pixel width of string
int  drawStr(const char* s, int x, uint32_t color);
void drawScroll(const char* txt, uint32_t color);  // advances global scrollOff
void flushDisplay();   // orientation + snake-map + brightness → NeoPixelBus.Show()

// Clock faces
void drawClockFace();
void drawDateFace();
void drawTempFace();
void drawIPFace();

// IP paged display — called when entering the IP face so the auto-return
// timer can cover a full page cycle.
void resetIpPages();
unsigned long ipFaceTimeoutMs();   // nPages * PAGE_IP_MS
