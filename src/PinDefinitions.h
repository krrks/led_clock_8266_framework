// PinDefinitions.h
// Hardware pin assignments — LED Matrix Clock (Wemos D1 Mini)
//
// ⚠  WS2812B data line moved to GPIO3 (RX/D9) for DMA/I2S output.
//    Rewire: unsolder D2→matrix data wire, solder RX(D9)→matrix data wire.
//    Serial.begin(..., SERIAL_TX_ONLY) frees GPIO3 for NeoPixelBus I2S DMA.
//    GPIO4 (D2) is now free.
//
// External connectors:
//   WS2812B data  GPIO3  RX/D9  ← was GPIO4/D2
//   BTN1 MODE     GPIO5  D1     (INPUT_PULLUP, active LOW)
//   BTN2 UP       GPIO14 D5     (INPUT_PULLUP, active LOW)
//   BTN3 DOWN     GPIO12 D6     (INPUT_PULLUP, active LOW)
//   BTN4 CONFIRM  GPIO13 D7     (INPUT_PULLUP, active LOW)
//   Onboard LED   GPIO2  D4     (active LOW, built-in)

#ifndef PINDEFINITIONS_H
#define PINDEFINITIONS_H

#include <Arduino.h>

// ── Buttons (INPUT_PULLUP, active LOW) ───────────────────────────────
#define BUTTON_1_PIN    5   // GPIO5  / D1 — MODE / SELECT
#define BUTTON_2_PIN   14   // GPIO14 / D5 — UP   / BRIGHTER
#define BUTTON_3_PIN   12   // GPIO12 / D6 — DOWN / DIMMER
#define BUTTON_4_PIN   13   // GPIO13 / D7 — CONFIRM / CANCEL

#ifndef BUTTON_PIN
#define BUTTON_PIN BUTTON_1_PIN
#endif

// ── Onboard LED (GPIO2 / D4, active LOW) ────────────────────────────
#define STATUS_LED_PIN  2

// ── WS2812B data (GPIO3 / RX / D9) — NeoPixelBus I2S DMA ───────────
#define LED_MATRIX_PIN  3

// ── Pin metadata table (for web settings display) ───────────────────
struct PinDef {
    uint8_t     gpio;
    const char* name;
    const char* description;
    const char* mode;
};

static const PinDef pinTable[] = {
    {  5, "BTN1",   "Mode / Select button",   "INPUT_PULLUP" },
    { 14, "BTN2",   "Up / Brighter button",   "INPUT_PULLUP" },
    { 12, "BTN3",   "Down / Dimmer button",   "INPUT_PULLUP" },
    { 13, "BTN4",   "Confirm / Cancel button","INPUT_PULLUP" },
    {  2, "LED",    "Status LED (onboard)",    "OUTPUT" },
    {  3, "WS2812", "LED matrix data (I2S DMA)","I2S_DMA" },
};
#define PIN_TABLE_COUNT (sizeof(pinTable) / sizeof(pinTable[0]))

#endif // PINDEFINITIONS_H
