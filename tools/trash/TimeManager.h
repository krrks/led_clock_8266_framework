// TimeManager.h
// Provides current time from NTP (via framework timeSync) when available,
// or from a manually set time tracked with millis() drift when NTP is not synced.
//
// Manual time is set from configManager fields and updated whenever the user
// saves a new time through the browser config page.

#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <Arduino.h>
#include <time.h>

class TimeManager {
public:
    TimeManager();

    // Set manual fallback time (called from configManager on save, or at init)
    // weekday: 1=Mon .. 7=Sun
    void setManualTime(int year, int month, int day,
                       int hour, int minute, int weekday);

    // True if the values passed differ from what was last set
    bool manualTimeChanged(int year, int month, int day,
                           int hour, int minute, int weekday) const;

    // Returns broken-down local time.
    // Uses NTP (system time) if timeSync.isSynced(), otherwise manual + millis drift.
    void getTimeComponents(int &hour, int &minute, int &second) const;
    void getDateComponents(int &year, int &month, int &day, int &weekday) const;

private:
    // Manual time state
    time_t        _manualBase;        // time_t value when manual was set
    unsigned long _manualBaseMillis;  // millis() snapshot at that moment
    bool          _manualSet;

    // Last-applied manual config snapshot (for change detection)
    int  _cfgYear, _cfgMonth, _cfgDay;
    int  _cfgHour, _cfgMinute, _cfgWeekday;

    // Returns current time_t from NTP or manual drift
    time_t _now() const;
};

#endif // TIMEMANAGER_H
