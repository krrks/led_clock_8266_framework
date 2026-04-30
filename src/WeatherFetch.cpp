// WeatherFetch.cpp — OWM weather HTTP client + recovery trigger
#include "WeatherFetch.h"
#include "AppState.h"

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "WiFiManager.h"
#include "configManager.h"

// ─── Recovery trigger ─────────────────────────────────────────────────────
void triggerRecovery() {
    Serial.println("[Recovery] writing RTC flag → restart");
    RTCData rtc = { RTC_MAGIC, 1 };
    ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
    delay(50);
    ESP.restart();
}

// ─── Weather fetch ────────────────────────────────────────────────────────
void fetchWeather() {
    if (!weatherEnabled) {
        Serial.println("[Weather] disabled — skip");
        return;
    }
    if (!wifiActive || WiFiManager.isCaptivePortal()) return;

    const char* city   = configManager.data.weatherCity;
    const char* apiKey = configManager.data.weatherApiKey;
    if (!city || !apiKey || strlen(city) == 0 || strlen(apiKey) < 8) {
        Serial.println("[Weather] city/key not configured — skip");
        return;
    }

    String url = F("http://api.openweathermap.org/data/2.5/weather?q=");
    url += city;
    url += F("&appid=");
    url += apiKey;
    url += F("&units=metric");

    Serial.printf("[Weather] fetching city=%s\n", city);

    WiFiClient  client;
    HTTPClient  http;
    http.setTimeout(8000);

    bool ok = false, cfgErr = false;

    if (http.begin(client, url)) {
        int code = http.GET();
        if (code == HTTP_CODE_OK) {
            JsonDocument doc;
            if (!deserializeJson(doc, http.getStream())) {
                weatherCode = (int16_t)(doc["weather"][0]["id"] | 0);
                weatherTemp = doc["main"]["temp"] | 0.0f;
                strlcpy(weatherDesc,
                        doc["weather"][0]["description"] | "N/A",
                        sizeof(weatherDesc));
                weatherFails = 0;
                ok = true;
                Serial.printf("[Weather] OK code=%d %.1f°C %s\n",
                              weatherCode, weatherTemp, weatherDesc);
            } else {
                Serial.println("[Weather] JSON parse error");
            }
        } else if (code == 401) {
            Serial.println("[Weather] 401 — invalid API key (check Configuration)");
            cfgErr = true;
        } else if (code == 404) {
            Serial.printf("[Weather] 404 — city not found: \"%s\" (check Configuration)\n", city);
            cfgErr = true;
        } else if (code == 400) {
            Serial.printf("[Weather] 400 — bad request, city=\"%s\"\n", city);
            cfgErr = true;
        } else {
            Serial.printf("[Weather] HTTP error %d\n", code);
        }
        http.end();
    } else {
        Serial.println("[Weather] http.begin() failed (check WiFi)");
    }

    if (!ok && !cfgErr) {
        weatherFails++;
        Serial.printf("[Weather] network fail %d / %d\n", weatherFails, WEATHER_FAIL_MAX);
        if (weatherFails >= WEATHER_FAIL_MAX) {
            Serial.printf("[Weather] %d consecutive failures → recovery\n", weatherFails);
            triggerRecovery();
        }
    }
}
