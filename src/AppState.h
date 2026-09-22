#pragma once
// AppState.h — shared enums, compile-time constants, extern declarations
// All global variables are *defined* in main.cpp and declared extern here.

#include <Arduino.h>
#include <time.h>
#include "LEDMatrixLayout.h"

// ─── Monochrome theme presets (indexed by configManager.data.colorIndex) ──
static const uint32_t COLOR_PRESETS[8] = {
    0xFFFFFF,  // 0 white (default)
    0xFFC800,  // 1 warm yellow
    0xFF0000,  // 2 red
    0x00CC00,  // 3 green
    0x00CCCC,  // 4 cyan
    0x0066FF,  // 5 blue
    0xFF6600,  // 6 orange
    0x8800CC,  // 7 purple
};

// ─── Firmware version (defined in main.cpp) ───────────────────────────────
extern const char FIRMWARE_VERSION[];

// ─── Compile-time constants ───────────────────────────────────────────────
static const unsigned long BOOT_WINDOW_MS   = 3000UL;
static const unsigned long IDLE_TIMEOUT_MS  = 30000UL;
static const unsigned long ACTIVE_REFRESH   = 1000UL;
static const unsigned long IDLE_REFRESH     = 30000UL;
static const unsigned long SCROLL_FRAME_MS  = 80UL;   // fallback; runtime uses configManager.data.scrollSpeed
static const unsigned long NTP_INTERVAL_MS  = 3600000UL;
static const unsigned long WEATHER_INT_MS   = 3600000UL;
static const int           DASH_INT_MS      = 5000;
static const unsigned long HEARTBEAT_MS     = 60000UL;
static const unsigned long FACE_TIMEOUT_MS  = 8000UL;  // non-clock faces auto-return to clock after this
static const unsigned long PAGE_IP_MS       = 3000UL;  // IP paged display: ms per page
static const unsigned long BOOT_ANIM_FRAME_MS = 125UL; // boot self-test animation frame (8 FPS)
static const unsigned long BOOT_ANIM_MAX_MS   = 45000UL;  // boot animation safety cap
static const unsigned long NTP_BOOT_TIMEOUT_MS= 20000UL; // NTP wait after WiFi up (slow DNS coverage)
static const unsigned long NTP_RETRY_DELAY_MS = 30000UL; // first NTP retry after boot if sync failed
static const unsigned long WEATHER_BOOT_DELAY_MS = 30000UL;  // first weather fetch after boot
static const unsigned long SETTINGS_TIMEOUT = 30000UL;
static const int           WEATHER_FAIL_MAX = 5;
static const uint32_t      HEAP_LOW_WATER   = 6000;  // below this → proactive recovery

// ─── Application mode ─────────────────────────────────────────────────────
enum AppMode : uint8_t { AM_NORMAL, AM_SETTINGS, AM_RECOVERY };

// ─── Display mode (normal mode only) ─────────────────────────────────────
enum DispMode : uint8_t { DM_CLOCK, DM_DATE, DM_TEMP, DM_IP, DM_COUNT };

// ─── Settings item indices ────────────────────────────────────────────────
// Time/date items (SI_HOUR..SI_WD) are hidden when NTP is synced.
enum SI : int {
    SI_HOUR=0, SI_MIN, SI_DAY, SI_MON, SI_YEAR, SI_WD,
    SI_TZ,
    SI_ROTATION,    // 0=0°  1=90°CW  2=180°  3=270°CW
    SI_FLIP,        // 0=none  1=H-flip  2=V-flip
    SI_SCROLLSPD,   // scroll ms per column (30-200)
    SI_BDIM, SI_BMED, SI_BBRT,
    SI_WX, SI_WIFI,
    SI_COUNT
};

// RTC flag — now owned by RecoveryManager. Legacy definition for reference:
// struct RTCData { uint32_t magic; uint32_t enterRecovery; };
// Use RecoveryManager::get().trigger() instead of direct RTC manipulation.

// ─────────────────────────────────────────────────────────────────────────
// Extern declarations — defined in main.cpp
// ─────────────────────────────────────────────────────────────────────────

// Logical pixel buffer — packed 0x00RRGGBB per pixel
extern uint32_t displayMatrix[MATRIX_HEIGHT][MATRIX_WIDTH];

extern AppMode  appMode;
extern DispMode dispMode;

extern bool     ntpSynced;
extern bool     weatherEnabled;
extern bool     wifiActive;

extern uint32_t mainColor;   // current theme colour (COLOR_PRESETS[colorIndex])

extern int16_t  weatherCode;
extern float    weatherTemp;
extern char     weatherDesc[40];   // longest OWM description is 32 chars ("thunderstorm with heavy drizzle")
extern int      weatherFails;

extern uint8_t  curRotation;   // 0=0°  1=90°CW  2=180°  3=270°CW
extern uint8_t  curFlip;       // 0=none  1=H-flip  2=V-flip

extern struct tm manualTm;
extern uint32_t  manualBase;

extern bool     pendingRedraw;
extern int      scrollOff;

// Settings mode state
extern int           settingsCursor;
extern int           settingsActive[20];
extern int           settingsCount;
extern unsigned long tSettingsEntry;

// Timer stamps
extern unsigned long tLastDisplay;
extern unsigned long tLastNtp;
extern unsigned long tLastWeather;
extern unsigned long tLastDash;
extern unsigned long tLastActivity;
extern unsigned long tLastHeart;
extern unsigned long tFaceUntil;

// ─── Shared helpers (defined in main.cpp) ─────────────────────────────────
// Triggers a pending redraw so next flushDisplay() picks up the new brightness.
void applyBrightness();

// Freeze watchdog pause/resume — upload handlers pause it while flashing so
// a slow OTA upload can't trip the watchdog mid-write.
void freezeWatchdogPause();
void freezeWatchdogResume();

// Deferred reboot — web callbacks (sys context) set this instead of calling
// ESP.restart() directly; the main loop performs the actual restart.
extern volatile bool gRebootRequested;
