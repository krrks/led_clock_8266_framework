#pragma once
// SettingsMode.h — on-device settings editor (BTN1 MODE 3 s to enter/exit)
//
// Navigation:
//   BTN1 MODE  click       → next item
//   BTN2 UP    click/hold  → value +1 / +5
//   BTN3 DOWN  click/hold  → value -1 / -5
//   BTN4 CONFIRM click     → save & exit
//   BTN4 CONFIRM hold 3 s  → cancel (restore snapshot)
//   BTN1 MODE  hold 3 s    → save & exit
//   auto-timeout 30 s      → save & exit

#include <Arduino.h>
#include "configManager.h"   // for configData type

// Timezone preset list
struct TzPreset { const char* label; const char* posix; };
extern const TzPreset TZ_PRESETS[];
extern const int      TZ_COUNT;
int findTzPreset(const char* posix);   // returns -1 if not in list

// Config snapshot — saved on settings entry, restored on cancel
extern configData settingsSnapshot;

// Build active setting indices (skips time/date when NTP is synced).
// Also saves settingsSnapshot.
void buildActiveSettings();

// Return a short display string for setting at SI_* index into buf.
void getSettingStr(int idx, char* buf, int len);

// Apply delta (+1/-1 for short, +5/-5 for medium) to a setting.
void adjustSetting(int idx, int delta);

// Draw settings overlay onto displayMatrix using ClockDisplay primitives.
void drawSettingsFace();

// Commit configManager, apply tz/time, return to AM_NORMAL.
void saveAndExitSettings();

// Restore snapshot, revert live state, return to AM_NORMAL without saving.
void cancelAndExitSettings();
