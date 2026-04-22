#pragma once
// BtnSimple.h — header-only, stateless 3-level button (no double-click)
//
// Press levels detected on each call to poll():
//   click  : pressed ≥ CLICK_DEBOUNCE_MS, released before LONG_PRESS_MS
//   lng    : held ≥ LONG_PRESS_MS   (fires once, while still held)
//   vlng   : held ≥ VLONG_PRESS_MS  (fires once, while still held)

#include <Arduino.h>

// Thresholds (shared with main)
static const unsigned long CLICK_DEBOUNCE_MS = 50UL;
static const unsigned long LONG_PRESS_MS     = 3000UL;
static const unsigned long VLONG_PRESS_MS    = 8000UL;

struct Btn {
    uint8_t  pin       = 0;
    bool     held      = false;
    uint32_t t0        = 0;
    bool     didLong   = false;
    bool     didVLong  = false;

    void begin(uint8_t p) {
        pin = p;
        held = false; t0 = 0; didLong = didVLong = false;
        pinMode(p, INPUT_PULLUP);
    }

    // Returns true if any event was generated.
    // Outputs: click / lng / vlng (all false when nothing new happened).
    bool poll(bool &click, bool &lng, bool &vlng) {
        click = lng = vlng = false;
        bool  dn  = (digitalRead(pin) == LOW);
        uint32_t now = millis();

        if (dn && !held) {
            held = true; t0 = now; didLong = didVLong = false;
        } else if (dn && held) {
            uint32_t dur = now - t0;
            if (!didVLong && dur >= VLONG_PRESS_MS) { didVLong = true; vlng  = true; }
            else if (!didLong && dur >= LONG_PRESS_MS)  { didLong  = true; lng   = true; }
        } else if (!dn && held) {
            uint32_t dur = now - t0;
            held = false;
            if (!didLong && dur >= CLICK_DEBOUNCE_MS) click = true;
        }
        return click || lng || vlng;
    }

    bool isHeld() const { return held; }
};
