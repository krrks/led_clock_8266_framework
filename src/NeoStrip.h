#pragma once
// NeoStrip.h — global NeoPixelBus DMA strip declaration
//
// WiFi-safe WS2812B output via ESP8266 I2S DMA peripheral.
// Output pin: GPIO3 (RX/D9) — hardwired by NeoEsp8266Dma800KbpsMethod.
// Serial must be started with SERIAL_TX_ONLY to free GPIO3 for I2S.
//
// Definition is in main.cpp; include this header everywhere the strip is used.

#include <NeoPixelBus.h>
#include "LEDMatrixLayout.h"  // for NUM_LEDS

// The single global LED strip — defined once in main.cpp
extern NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> neoStrip;
