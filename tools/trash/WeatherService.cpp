// WeatherService.cpp
#include "WeatherService.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

WeatherService::WeatherService()
    : _conditionCode(0), _temperature(0.0f),
      _hasFreshData(false), _failCount(0) {}

void WeatherService::begin() {
    _conditionCode = 0;
    _temperature   = 0.0f;
    _hasFreshData  = false;
    _failCount     = 0;
}

bool WeatherService::fetch(const char *city, const char *apiKey) {
    if (!city || !apiKey || strlen(city) == 0 || strlen(apiKey) == 0) {
        Serial.println("[Weather] city or apiKey not configured");
        return false;
    }

    // Build URL  (OWM free tier: plain HTTP is fine for weather data)
    String url = "http://api.openweathermap.org/data/2.5/weather?q=";
    url += city;
    url += "&appid=";
    url += apiKey;
    url += "&units=metric";

    WiFiClient   client;
    HTTPClient   http;

    http.setTimeout(8000);
    if (!http.begin(client, url)) {
        Serial.println("[Weather] http.begin() failed");
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[Weather] HTTP error: %d\n", httpCode);
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    // Parse JSON
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        Serial.printf("[Weather] JSON parse error: %s\n", err.c_str());
        return false;
    }

    _conditionCode = doc["weather"][0]["id"] | 0;
    _temperature   = doc["main"]["temp"]     | 0.0f;
    _hasFreshData  = true;

    Serial.printf("[Weather] OK  code=%d  temp=%.1f°C\n",
                  _conditionCode, _temperature);
    return true;
}

// ── Condition code → display colour (packed 0x00RRGGBB) ─────────────────────
uint32_t WeatherService::_codeToColor(int code) {
    if (code == 0)                   return 0x444444; // no data: dim grey
    if (code == 800)                 return 0xFFCC00; // clear sky: yellow
    if (code >= 801 && code <= 802)  return 0xAAAA00; // few clouds: pale yellow
    if (code >= 803 && code <= 804)  return 0x888888; // overcast: grey
    if (code >= 300 && code <= 321)  return 0x6699FF; // drizzle: light blue
    if (code >= 500 && code <= 531)  return 0x0044FF; // rain: blue
    if (code >= 200 && code <= 232)  return 0x8800CC; // thunderstorm: purple
    if (code >= 600 && code <= 622)  return 0x00FFFF; // snow: cyan
    if (code >= 700 && code <= 781)  return 0x555555; // fog/mist: dim grey
    return 0xFFFFFF;                                   // unknown: white
}
