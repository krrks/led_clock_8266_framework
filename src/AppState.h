#pragma once
// AppState.h — shared enums, compile-time constants, extern declarations
// All global variables are *defined* in main.cpp and declared extern here.

#include <Arduino.h>
#include <time.h>
#include "LEDMatrixLayout.h"

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
static const unsigned long IP_SHOW_MS       = 8000UL;
static const unsigned long SETTINGS_TIMEOUT = 30000UL;
static const int           WEATHER_FAIL_MAX = 5;

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

// ─── RTC flag ────────────────────────────────────────────────────────────
#define RTC_MAGIC 0xC10CFA11UL
struct RTCData { uint32_t magic; uint32_t enterRecovery; };

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

extern int16_t  weatherCode;
extern float    weatherTemp;
extern char     weatherDesc[32];
extern int      weatherFails;

extern uint8_t  curRotation;   // 0=0°  1=90°CW  2=180°  3=270°CW
extern uint8_t  curFlip;       // 0=none  1=H-flip  2=V-flip

extern struct tm manualTm;
extern uint32_t  manualBase;

extern bool     pendingRedraw;
extern int      scrollOff;

// Error state — set by setError(), cleared by clearError()
extern char         errorMessage[128];
extern unsigned long tErrorSet;

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
extern unsigned long tShowIPUntil;
extern unsigned long tLedBlink;
extern bool          ledBlinkState;

// ─── Shared helpers (defined in main.cpp) ─────────────────────────────────
// Triggers a pending redraw so next flushDisplay() picks up the new brightness.
void applyBrightness();

// Set an error message, switch to AM_RECOVERY, and log to Serial — no reboot.
// Safe to call from any module (WeatherFetch, button handlers, etc.).
// Repeated calls update the message but do NOT reset tErrorSet.
void setError(const char* msg);

// Clear error state and return to AM_NORMAL. Resets weatherFails counter.
void clearError();
