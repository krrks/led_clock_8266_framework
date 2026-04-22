// PinDefinitions.h
// Hardware pin assignments — LED Matrix Clock (Wemos D1 Mini)
//
// External connectors:
//   WS2812B data  GPIO4  D2
//   BTN1 MODE     GPIO5  D1   (INPUT_PULLUP, active LOW)
//   BTN2 UP       GPIO14 D5   (INPUT_PULLUP, active LOW)
//   BTN3 DOWN     GPIO12 D6   (INPUT_PULLUP, active LOW)
//   Onboard LED   GPIO2  D4   (active LOW, built-in)

#ifndef PINDEFINITIONS_H
#define PINDEFINITIONS_H

#include <Arduino.h>

// ── Buttons (INPUT_PULLUP, active LOW) ───────────────────────────────
#define BUTTON_1_PIN    5   // GPIO5  / D1 — MODE / SELECT
#define BUTTON_2_PIN   14   // GPIO14 / D5 — UP   / BRIGHTER
#define BUTTON_3_PIN   12   // GPIO12 / D6 — DOWN / DIMMER

// Legacy alias (keeps any old code referencing BUTTON_PIN compiling)
#ifndef BUTTON_PIN
#define BUTTON_PIN BUTTON_1_PIN
#endif

// ── Onboard LED (GPIO2 / D4, active LOW) ────────────────────────────
#define STATUS_LED_PIN  2

// ── WS2812B data line (GPIO4 / D2) ──────────────────────────────────
#define LED_MATRIX_PIN  4

// NUM_LEDS is derived from MATRIX_WIDTH * MATRIX_HEIGHT in LEDMatrixLayout.h

#endif // PINDEFINITIONS_H
