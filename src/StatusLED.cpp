// StatusLED.cpp
#include "StatusLED.h"

StatusLED::StatusLED(uint8_t pin, bool activeLow)
    : _pin(pin), _activeLow(activeLow),
      _mode(OFF), _state(false), _lastToggle(0) {}

void StatusLED::begin() {
    pinMode(_pin, OUTPUT);
    _write(false);
}

void StatusLED::setMode(Mode mode) {
    _mode = mode;
    _lastToggle = 0; // reset blink timer
    switch (mode) {
        case OFF:      _write(false); break;
        case SOLID_ON: _write(true);  break;
        default: break; // blink modes handled in update()
    }
}

void StatusLED::update() {
    unsigned long interval = 0;
    switch (_mode) {
        case FAST_BLINK: interval = 100;  break;
        case SLOW_BLINK: interval = 1000; break;
        default: return;
    }

    unsigned long now = millis();
    if (now - _lastToggle >= interval) {
        _lastToggle = now;
        _state = !_state;
        _write(_state);
    }
}

void StatusLED::_write(bool on) {
    // GPIO2 is active LOW: HIGH = LED off, LOW = LED on
    digitalWrite(_pin, _activeLow ? !on : on);
}
