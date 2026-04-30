# Hardware & Pin Mapping

## Components

- **Wemos D1 Mini** (ESP8266, 160MHz, 4MB Flash)
- **32×8 WS2812B LED Matrix** (256 pixels, column-snake wiring)
- **4 push buttons** (MODE, UP, DOWN, CONFIRM)
- **5V USB power** (smartphone charger)

## Pin Assignments

| Signal         | GPIO | D1 Mini pin | Mode | Notes |
|----------------|------|-------------|------|-------|
| BTN1 MODE      | 5    | D1          | INPUT_PULLUP | Active LOW |
| BTN2 UP        | 14   | D5          | INPUT_PULLUP | Active LOW |
| BTN3 DOWN      | 12   | D6          | INPUT_PULLUP | Active LOW |
| BTN4 CONFIRM   | 13   | D7          | INPUT_PULLUP | Active LOW |
| Status LED     | 2    | D4          | OUTPUT | Active LOW (built-in) |
| WS2812B data   | 3    | RX/D9       | I2S DMA | NeoPixelBus direct |

> **IMPORTANT**: The WS2812B data line MUST be on GPIO3 (RX/D9) for I2S DMA output.
> If upgrading from v1.007 (GPIO4/D2), rewire: unsolder D2 → matrix data, solder RX → matrix data.

## Wiring Buttons

Connect one terminal of each button to the GPIO pin, other terminal to GND.
Internal pull-up is enabled in software — no external resistors needed.
