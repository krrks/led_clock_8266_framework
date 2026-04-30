# Display Modes & Layout

## LED Matrix Layout

```
Col  0- 4  Hour tens digit
Col  5     space
Col  6-10  Hour units digit
Col 11     space
Col 12     colon ( : )
Col 13     space
Col 14-18  Minute tens digit
Col 19     space
Col 20-24  Minute units digit
Col 25     gap
Col 26     DATE column   (1px per 7-day week, white, bottom->top)
Col 27     gap
Col 28     WEEKDAY column (Mon=1px...Sun=7px, cyan weekday / orange weekend)
Col 29     gap
Col 30     WEATHER column (4px, colour = OWM condition)
Col 31     trailing pad
```

Columns 26-30 are "info bars" that provide context at a glance.

## Display Modes

Press **BTN1 (MODE)** to cycle:

| Mode  | Content | Colour |
|-------|---------|--------|
| CLOCK | HH:MM + info bars (default) | White + colour bars |
| DATE  | Static date, e.g. `22APR` | Green |
| TEMP  | Static temperature, e.g. `25.1C` | Yellow |
| IP    | Scrolling WiFi IP or "NO WIFI" | Cyan |

After boot WiFi connect, IP mode is shown for 8 seconds, then returns to CLOCK.

## Display Orientation

Rotation and flip are **independent** and **combinable**:

**Rotation:**
| Value | Label | Description |
|-------|-------|-------------|
| 0 | 0° | Standard horizontal mount |
| 1 | 90°CW | Portrait (scaled) |
| 2 | 180° | Upside-down |
| 3 | 270°CW | Portrait, other way |

**Flip:**
| Value | Label | Description |
|-------|-------|-------------|
| 0 | None | No mirroring |
| 1 | H-Flip | Mirror left-right |
| 2 | V-Flip | Mirror top-bottom |

Both can be set via web Configuration or on-device settings mode.

## Weather Colours

| Condition     | OWM Code Range | LED Colour  |
|---------------|----------------|-------------|
| Thunderstorm  | 200-299        | Purple      |
| Drizzle       | 300-399        | Light blue  |
| Rain          | 500-599        | Blue        |
| Snow          | 600-699        | Icy blue    |
| Fog / mist    | 700-799        | Grey        |
| Clear sky     | 800            | Yellow      |
| Few/scattered | 801-802        | Pale yellow |
| Overcast      | 803-804        | Grey        |

Full code list: <https://openweathermap.org/weather-conditions>

## Scroll Speed

Controls all scrolling text animation (IP address, recovery text, settings items wider than matrix).
Range: 30-200 ms/column. Default: 80 ms/column. Adjustable via web or on-device settings.
