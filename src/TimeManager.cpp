// TimeManager.cpp
#include "TimeManager.h"
#include <timeSync.h>     // note the < > instead of " "   // framework timeSync singleton

TimeManager::TimeManager()
    : _manualBase(0), _manualBaseMillis(0), _manualSet(false),
      _cfgYear(0), _cfgMonth(0), _cfgDay(0),
      _cfgHour(255), _cfgMinute(255), _cfgWeekday(0) {}

// ── Public ──────────────────────────────────────────────────────────────────

void TimeManager::setManualTime(int year, int month, int day,
                                int hour, int minute, int weekday) {
    // weekday param: 1=Mon..7=Sun → tm_wday: 0=Sun..6=Sat
    int tmWday = (weekday == 7) ? 0 : weekday;

    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = 0;
    t.tm_wday = tmWday;

    _manualBase       = mktime(&t);
    _manualBaseMillis = millis();
    _manualSet        = true;

    // Remember what was applied so we can detect changes
    _cfgYear    = year;
    _cfgMonth   = month;
    _cfgDay     = day;
    _cfgHour    = hour;
    _cfgMinute  = minute;
    _cfgWeekday = weekday;
}

bool TimeManager::manualTimeChanged(int year, int month, int day,
                                    int hour, int minute, int weekday) const {
    return (year    != _cfgYear    ||
            month   != _cfgMonth   ||
            day     != _cfgDay     ||
            hour    != _cfgHour    ||
            minute  != _cfgMinute  ||
            weekday != _cfgWeekday);
}

void TimeManager::getTimeComponents(int &hour, int &minute, int &second) const {
    time_t t = _now();
    if (t == 0) { hour = minute = second = 0; return; }
    struct tm *tm = localtime(&t);
    if (!tm)   { hour = minute = second = 0; return; }
    hour   = tm->tm_hour;
    minute = tm->tm_min;
    second = tm->tm_sec;
}

void TimeManager::getDateComponents(int &year, int &month,
                                    int &day, int &weekday) const {
    time_t t = _now();
    if (t == 0) { year = 2025; month = 1; day = 1; weekday = 1; return; }
    struct tm *tm = localtime(&t);
    if (!tm)   { year = 2025; month = 1; day = 1; weekday = 1; return; }
    year    = tm->tm_year + 1900;
    month   = tm->tm_mon  + 1;
    day     = tm->tm_mday;
    // Convert 0=Sun..6=Sat → 1=Mon..7=Sun
    weekday = (tm->tm_wday == 0) ? 7 : tm->tm_wday;
}

// ── Private ─────────────────────────────────────────────────────────────────

time_t TimeManager::_now() const {
    if (timeSync.isSynced()) {
        return time(nullptr);   // system clock set by NTP
    }
    if (_manualSet) {
        unsigned long elapsed = (millis() - _manualBaseMillis) / 1000UL;
        return _manualBase + (time_t)elapsed;
    }
    return 0;
}
