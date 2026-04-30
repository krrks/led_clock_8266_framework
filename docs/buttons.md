# Button Reference

## Physical Buttons

| Button | GPIO | D1 Mini | Function |
|--------|------|---------|----------|
| BTN1   | 5    | D1      | MODE / SELECT |
| BTN2   | 14   | D5      | UP / BRIGHTER |
| BTN3   | 12   | D6      | DOWN / DIMMER |
| BTN4   | 13   | D7      | CONFIRM / CANCEL |

## Normal Mode

| Button | Action | Effect |
|--------|--------|--------|
| **BTN1 click** | Cycle display mode | CLOCK → DATE → TEMP → IP |
| **BTN1 3s** | Enter settings | Opens on-device settings editor |
| **BTN1 8s** | Enter recovery | Triggers recovery mode |
| **BTN2 click** | Brightness up | +1 step (dim→med→bright) |
| **BTN2 3s** | Force refresh | NTP re-sync + weather re-fetch |
| **BTN3 click** | Brightness down | -1 step |
| **BTN3 3s** | Toggle weather | Enable/disable weather display |
| **BTN4 click** | Show IP | Scrolling IP for 8 seconds |

## Settings Mode

| Button | Action | Effect |
|--------|--------|--------|
| **BTN1 click** | Next item | Cycle through settings |
| **BTN1 3s** | Save & exit | Commit changes, return to normal |
| **BTN2 click** | +1 | Increment current value |
| **BTN2 3s** | +5 | Fast increment |
| **BTN3 click** | -1 | Decrement current value |
| **BTN3 3s** | -5 | Fast decrement |
| **BTN4 click** | Save & exit | Commit changes |
| **BTN4 3s** | Cancel | Restore snapshot, discard changes |

Auto-save after 30 seconds of inactivity.

## Recovery Mode

Exited via web UI (`/api/exit`) or button hold. See [Recovery Mode](recovery.md).

## Boot Window

Hold **BTN1** during the first 3 seconds after power-on to enter recovery mode.
The onboard LED fast-blinks during this window.
