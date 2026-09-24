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

## Free & Safe GPIOs

| GPIO | D1 Mini | Use | Notes |
|------|---------|-----|-------|
| 4  | D2  | Input / output | **Best choice** — no strapping/UART conflict |
| 16 | D0  | Output only | RTC/WAKE pin: no internal pull-up, no interrupt |
| 15 | D8  | Input / output | MTDO strap — keep LOW at boot (module already pulls down) |
| A0 | ADC | Analog input | 0–1 V only; use a voltage divider for battery monitoring |

## Boot / Flashing Strapping Pins

These are sampled at power-on; keep them in the required state, or the chip
enters the wrong mode / fails to boot:

| GPIO | Role | Required at boot |
|------|------|------------------|
| 0  | Boot mode (LOW → UART flash) | HIGH (module pull-up) |
| 2  | Boot mode + status LED | HIGH (module pull-up) |
| 15 | MTDO | LOW (module pull-down) |
| 12 | MTDI (flash voltage) | LOW — don't add an external pull-up |
| 1  | UART TX (boot ROM log) | — |
| 3  | UART RX + matrix data | — |

GPIO6–11 are wired to the SPI flash and are not broken out — unavailable.
