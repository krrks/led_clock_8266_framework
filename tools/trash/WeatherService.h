// WeatherService.h
// Fetches current weather from OpenWeatherMap (free, HTTP) and caches the result.
// Exposes a colour value (packed 0x00RRGGBB) for the weather info column on the matrix.
//
// Fail counter: main.cpp calls incrementFailCount() on repeated failures.
// After WEATHER_FAIL_LIMIT consecutive failures the clock enters recovery mode.

#ifndef WEATHERSERVICE_H
#define WEATHERSERVICE_H

#include <Arduino.h>

#define WEATHER_FAIL_LIMIT  5   // consecutive fetch failures before recovery

class WeatherService {
public:
    WeatherService();
    void begin();

    // Fetch from OWM; returns true on success.
    // Requires active WiFi connection.
    bool fetch(const char *city, const char *apiKey);

    int      getConditionCode()  const { return _conditionCode; }
    float    getTemperature()    const { return _temperature; }
    uint32_t getConditionColor() const { return _codeToColor(_conditionCode); }
    bool     hasFreshData()      const { return _hasFreshData; }

    void incrementFailCount() { _failCount++; }
    void resetFailCount()     { _failCount = 0; }
    int  getFailCount()       const { return _failCount; }

private:
    int      _conditionCode;
    float    _temperature;
    bool     _hasFreshData;
    int      _failCount;

    static uint32_t _codeToColor(int code);
};

#endif // WEATHERSERVICE_H
