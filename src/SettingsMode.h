#pragma once
// SettingsMode.h — on-device settings editor (BTN_MODE 3 s to enter/exit)
//
// User navigates items with BTN_MODE click,
// adjusts value with BTN_UP (+1) / BTN_DOWN (-1).
// Auto-saves and exits after SETTINGS_TIMEOUT or another 3 s long press.

#include <Arduino.h>

// Timezone preset list (label shown on LED, posix stored in config)
struct TzPreset { const char* label; const char* posix; };
extern const TzPreset TZ_PRESETS[];
extern const int      TZ_COUNT;
int findTzPreset(const char* posix);   // returns -1 if not in list

// Build active setting indices (skips time/date items when NTP is synced)
void buildActiveSettings();

// Return the display string for a given SI_* index into buf (len bytes)
void getSettingStr(int idx, char* buf, int len);

// Apply delta (+1/-1) to the setting at SI_* idx; saves brightness immediately
void adjustSetting(int idx, int delta);

// Draw settings overlay onto displayMatrix
void drawSettingsFace();

// Commit configManager, update timezone + manual time base, return to AM_NORMAL
void saveAndExitSettings();
