#!build
## v1.008 — NeoPixelBus DMA (WiFi-safe LED output) + build cache fix

### Root-cause fix: WS2812B output now WiFi-safe
- **Replaced FastLED** (bit-bang GPIO, disables interrupts ~300 µs per frame)
  with **NeoPixelBus DMA/I2S** (`NeoEsp8266Dma800KbpsMethod`)
- I2S DMA runs entirely in hardware — never disables WiFi interrupts
- Fixes: "only first pixel lights", "random single pixel", "no response" on button press
  that were all caused by WiFi ISR clobbering the FastLED bit-bang timing

### ⚠ Hardware rewire required
- Data line moved: **D2 (GPIO4) → RX/D9 (GPIO3)**
- NeoPixelBus DMA hardwires to GPIO3 (I2S data-out)
- `Serial.begin()` switched to `SERIAL_TX_ONLY` — frees GPIO3 for DMA,
  TX (D8/GPIO1) still works normally for serial monitor
- All other pins unchanged (buttons, onboard LED)

### CI: build cache now works
- Added `PLATFORMIO_BUILD_CACHE_DIR=~/.pio/build_cache` env var to build job
- Added `actions/cache` step for `~/.pio/build_cache`
- This is what PlatformIO requires to use its shared object cache;
  caching `.pio/build/d1_mini` alone was insufficient and produced the
  "Advanced Memory Usage ... won't use cache" warning

### Code cleanup
- Removed `FastLED` from `lib_deps` and all `#include <FastLED.h>` references
- `leds[]` (CRGB array) removed; `NeoPixelBus neoStrip` is the sole LED object
- `applyBrightness()` simplified — brightness now applied per-pixel in `flushDisplay()`
  by scaling RGB channels, no global FastLED brightness register needed
- Legacy source files (`DisplayManager`, `ClockFace`, etc.) excluded from build
  via `src_filter` in `platformio.ini` (files kept for reference)
- New `NeoStrip.h` declares the global strip for inclusion across modules
