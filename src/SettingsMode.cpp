// SettingsMode.cpp — on-device settings editor
#include "SettingsMode.h"
#include "AppState.h"
#include "ClockDisplay.h"

#include "configManager.h"
#include "timeSync.h"

// ─── Config snapshot (for cancel) ────────────────────────────────────────
configData settingsSnapshot;

// ─── Timezone preset table ────────────────────────────────────────────────
const TzPreset TZ_PRESETS[] = {
    {"UTC",    "UTC0"},
    {"GMT",    "GMT0BST,M3.5.0/1,M10.5.0"},
    {"CET+1",  "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"EET+2",  "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"MSK+3",  "MSK-3"},
    {"IST+5",  "IST-5:30"},
    {"PKT+5",  "PKT-5"},
    {"HKT+8",  "HKT-8"},
    {"CST+8",  "CST-8"},
    {"JST+9",  "JST-9"},
    {"AEST",   "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"NZST",   "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"EST-5",  "EST5EDT,M3.2.0,M11.1.0"},
    {"CST-6",  "CST6CDT,M3.2.0,M11.1.0"},
    {"MST-7",  "MST7MDT,M3.2.0,M11.1.0"},
    {"PST-8",  "PST8PDT,M3.2.0,M11.1.0"},
    {"ART-3",  "ART3"},
    {"BRT-3",  "BRT3"},
};
const int TZ_COUNT = (int)(sizeof(TZ_PRESETS) / sizeof(TZ_PRESETS[0]));

int findTzPreset(const char* posix) {
    for (int i = 0; i < TZ_COUNT; i++)
        if (strcmp(TZ_PRESETS[i].posix, posix) == 0) return i;
    return -1;
}

// ─── Active-item list ─────────────────────────────────────────────────────
void buildActiveSettings() {
    settingsSnapshot = configManager.data;  // save for potential cancel
    settingsCount = 0;
    for (int i = 0; i < SI_COUNT; i++) {
        if (i <= SI_WD && ntpSynced) continue;  // hide time/date when NTP active
        settingsActive[settingsCount++] = i;
    }
}

// ─── Item display string ──────────────────────────────────────────────────
void getSettingStr(int idx, char* buf, int len) {
    static const char* WDN[] = {"Mon","Tue","Wed","Thu","Fri","Sat","Sun"};
    auto& d = configManager.data;
    switch (idx) {
        case SI_HOUR: snprintf(buf, len, "H:%d",   d.manualHour);    break;
        case SI_MIN:  snprintf(buf, len, "m:%02d", d.manualMinute);  break;
        case SI_DAY:  snprintf(buf, len, "d:%d",   d.manualDay);     break;
        case SI_MON:  snprintf(buf, len, "Mo:%d",  d.manualMonth);   break;
        case SI_YEAR: snprintf(buf, len, "Y:%d",   d.manualYear);    break;
        case SI_WD: {
            int w = constrain((int)d.manualWeekday, 1, 7) - 1;
            snprintf(buf, len, "Wd:%s", WDN[w]);
        } break;
        case SI_TZ: {
            int pi = findTzPreset(d.timezone);
            snprintf(buf, len, "TZ:%s", (pi >= 0) ? TZ_PRESETS[pi].label : "CUST");
        } break;
        case SI_ROTATION: {
            static const char* ROT[] = {"RO:N","RO:90","RO:180","RO:270"};
            strlcpy(buf, ROT[constrain((int)d.rotation, 0, 3)], len);
        } break;
        case SI_FLIP: {
            static const char* FLP[] = {"FL:N","FL:H","FL:V"};
            strlcpy(buf, FLP[constrain((int)d.flip, 0, 2)], len);
        } break;
        case SI_SCROLLSPD: snprintf(buf, len, "SP:%d", d.scrollSpeed); break;
        case SI_BDIM: snprintf(buf, len, "DM:%d", d.brightDim); break;
        case SI_BMED: snprintf(buf, len, "MD:%d", d.brightMed); break;
        case SI_BBRT: snprintf(buf, len, "BR:%d", d.brightBrt); break;
        case SI_WX:   snprintf(buf, len, "Wx:%s", d.defaultWeather ? "ON":"OFF"); break;
        case SI_WIFI: snprintf(buf, len, "Wi:%s", d.wifiEnabled    ? "ON":"OFF"); break;
        default:      strlcpy(buf, "???", len); break;
    }
}

// ─── Adjust one setting item by delta ────────────────────────────────────
// delta: ±1 for short press, ±5 for medium press
void adjustSetting(int idx, int delta) {
    auto& d = configManager.data;
    switch (idx) {
        case SI_HOUR: d.manualHour    = (uint8_t)constrain((int)d.manualHour    + delta, 0, 23); break;
        case SI_MIN:  d.manualMinute  = (uint8_t)constrain((int)d.manualMinute  + delta, 0, 59); break;
        case SI_DAY:  d.manualDay     = (uint8_t)constrain((int)d.manualDay     + delta, 1, 31); break;
        case SI_MON:  d.manualMonth   = (uint8_t)constrain((int)d.manualMonth   + delta, 1, 12); break;
        case SI_YEAR: d.manualYear    = (uint16_t)constrain((int)d.manualYear   + delta, 2024, 2099); break;
        case SI_WD:   d.manualWeekday = (uint8_t)constrain((int)d.manualWeekday + delta, 1, 7);  break;
        case SI_TZ: {
            int pi = findTzPreset(d.timezone);
            if (pi < 0) pi = 0;
            pi = (pi + delta + TZ_COUNT) % TZ_COUNT;
            strlcpy(d.timezone, TZ_PRESETS[pi].posix, sizeof(d.timezone));
        } break;
        case SI_ROTATION:
            d.rotation = (uint8_t)((d.rotation + delta + 4) % 4);
            curRotation = d.rotation;
            break;
        case SI_FLIP:
            d.flip = (uint8_t)((d.flip + delta + 3) % 3);
            curFlip = d.flip;
            break;
        case SI_SCROLLSPD:
            d.scrollSpeed = (uint8_t)constrain((int)d.scrollSpeed + delta * 5, 30, 200);
            break;
        case SI_BDIM:
            d.brightDim = (uint8_t)constrain((int)d.brightDim + delta * 5, 1, 200);
            applyBrightness();
            break;
        case SI_BMED:
            d.brightMed = (uint8_t)constrain((int)d.brightMed + delta * 5, 1, 255);
            applyBrightness();
            break;
        case SI_BBRT:
            d.brightBrt = (uint8_t)constrain((int)d.brightBrt + delta * 5, 1, 255);
            applyBrightness();
            break;
        case SI_WX:
            d.defaultWeather = !d.defaultWeather;
            weatherEnabled   =  d.defaultWeather;
            break;
        case SI_WIFI:
            d.wifiEnabled = !d.wifiEnabled;
            break;
    }
    pendingRedraw = true;
}

// ─── Draw current item on the display ────────────────────────────────────
void drawSettingsFace() {
    if (settingsCount == 0) return;
    char buf[24];
    getSettingStr(settingsActive[settingsCursor], buf, sizeof(buf));
    int w = strPxW(buf);
    if (w <= MATRIX_WIDTH) {
        drawStr(buf, (MATRIX_WIDTH - w) / 2, C_CYAN);
    } else {
        drawScroll(buf, C_CYAN);
    }
}

// ─── Save & exit ─────────────────────────────────────────────────────────
void saveAndExitSettings() {
    // Apply timezone immediately
    const char* tz = (strlen(configManager.data.timezone) > 0)
                     ? configManager.data.timezone : "HKT-8";
    timeSync.begin(tz);

    // Re-anchor manual time base (only when NTP unavailable)
    if (!ntpSynced) {
        memset(&manualTm, 0, sizeof(manualTm));
        manualTm.tm_hour  = configManager.data.manualHour;
        manualTm.tm_min   = configManager.data.manualMinute;
        manualTm.tm_mday  = configManager.data.manualDay;
        manualTm.tm_mon   = configManager.data.manualMonth - 1;
        manualTm.tm_year  = configManager.data.manualYear - 1900;
        manualTm.tm_isdst = -1;
        mktime(&manualTm);
        manualBase = millis();
    }

    configManager.save();
    Serial.println("[Settings] saved — returning to normal");
    appMode       = AM_NORMAL;
    scrollOff     = 0;
    pendingRedraw = true;
    tLastActivity = millis();
}

// ─── Cancel & exit (restore snapshot) ────────────────────────────────────
void cancelAndExitSettings() {
    // Restore config from snapshot
    configManager.data = settingsSnapshot;
    // Revert live display state
    curRotation    = configManager.data.rotation;
    curFlip        = configManager.data.flip;
    weatherEnabled = configManager.data.defaultWeather;
    applyBrightness();
    // Exit without saving to EEPROM
    Serial.println("[Settings] cancelled — config restored (not saved)");
    appMode       = AM_NORMAL;
    scrollOff     = 0;
    pendingRedraw = true;
    tLastActivity = millis();
}
