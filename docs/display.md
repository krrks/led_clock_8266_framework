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
Col 26-31  WEATHER icon (6×7, clock mode only)
Row 7      DAY indicator (clock mode only): column = day of month,
           colour = weekday group (see below)
```

## Display Modes

Press **BTN1 (MODE)** to cycle. All text uses the selected theme colour
(see [Monochrome Theme](#monochrome-theme)):

| Mode  | Content | Notes |
|-------|---------|-------|
| CLOCK | HH:MM + weather icon + day indicator | Default |
| DATE  | Static date `MM-DD`, e.g. `09-17` | |
| TEMP  | Static temperature, e.g. `25.1C` | |
| IP    | Paged IP, width-fit, e.g. `192.` → `168.` → `3.47` | 3 s per page |

Non-clock modes auto-return to CLOCK after 8 seconds. After boot WiFi
connect, IP mode is shown for 8 seconds, then returns to CLOCK.

## Day Indicator (row 7)

A single pixel on the bottom row of the clock face:

- **Column** = day of month (1st → col 0 … 31st → col 30)
- **Colour** = weekday group: Mon/Tue **green**, Wed/Thu **blue**,
  Fri/Sat/Sun **red**

## Weather Icon (cols 26-31)

6×7 glyph drawn in the theme colour, mapped from OWM condition code:

| Glyph | Condition | OWM Code Range |
|-------|-----------|----------------|
| Sun | Clear | 800 |
| Small cloud | Few clouds | 801 |
| Cloud | Scattered | 802 |
| Big cloud | Broken / overcast | 803-804 |
| Cloud + 2 drops | Shower / drizzle | 300-321 |
| Cloud + 3 drops | Rain | 500-531 |
| Cloud + bolt | Thunderstorm | 200-232 |
| Cloud + dots | Snow | 600-622 |
| Lines | Mist / fog / haze | 700-781 |

Icon is off when weather is disabled or no data (code 0).

## Monochrome Theme

All faces (clock, date, temp, IP, weather icon) share one theme colour,
cycled with **BTN3** and persisted in `colorIndex` config (also editable
on the web Settings page):

| Index | Colour |
|-------|--------|
| 0 | White (default) |
| 1 | Warm yellow |
| 2 | Red |
| 3 | Green |
| 4 | Cyan |
| 5 | Blue |
| 6 | Orange |
| 7 | Purple |

The day indicator keeps its own weekday colouring regardless of theme.

## Display Orientation

Rotation is configurable (web or on-device settings). **Flip is hardcoded**
to V-flip in `ClockDisplay.cpp` (`applyOrientation`) — the panel is mounted
upside-down; the config `flip` value is ignored.

| Rotation | Label | Description |
|----------|-------|-------------|
| 0 | 0° | Standard horizontal mount |
| 1 | 90°CW | Portrait (scaled) |
| 2 | 180° | Upside-down |
| 3 | 270°CW | Portrait, other way |

## Scroll Speed

Controls all scrolling text animation (recovery text, settings items wider
than matrix). Range: 30-200 ms/column. Default: 80 ms/column. Adjustable
via web or on-device settings.
