// StatusLED.h
// Manages the onboard LED (GPIO2, active LOW).
//
// Modes:
//   OFF        - LED completely dark (normal clock mode)
//   FAST_BLINK - 100 ms on/off  (boot window, waiting for WiFi)
//   SLOW_BLINK - 1000 ms on/off (recovery mode)
//   SOLID_ON   - always on      (OTA in progress)

#ifndef STATUSLED_H
#define STATUSLED_H

#include <Arduino.h>

class StatusLED {
public:
    enum Mode { OFF, FAST_BLINK, SLOW_BLINK, SOLID_ON };

    explicit StatusLED(uint8_t pin, bool activeLow = true);

    void begin();
    void setMode(Mode mode);
    Mode getMode() const { return _mode; }

    // Call every loop() iteration
    void update();

private:
    uint8_t       _pin;
    bool          _activeLow;
    Mode          _mode;
    bool          _state;
    unsigned long _lastToggle;

    void _write(bool on);
};

#endif // STATUSLED_H
