// WeatherFetch.cpp — OWM weather HTTP client + recovery trigger
#include "WeatherFetch.h"
#include "AppState.h"
#include "recovery/RecoveryManager.h"

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "wifi/WiFiService.h"
#include "config/ConfigManager.h"

// Local-only secrets (gitignored); public clones fall back to empty key.
#if __has_include("config/Secrets.h")
#include "config/Secrets.h"
#endif
#ifndef WEATHER_API_KEY
#define WEATHER_API_KEY ""
#endif

// ─── Recovery trigger ─────────────────────────────────────────────────────
void triggerRecovery() {
    RecoveryManager::get().trigger();
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

    // Fallback for this build: Guangzhou + key from local Secrets.h
    // (empty in public builds — weather then stays off until configured).
    if (!city || strlen(city) == 0)           city   = "Guangzhou,CN";
    if (!apiKey || strlen(apiKey) < 8)        apiKey = WEATHER_API_KEY;

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

    bool ok = false, cfgErr = false, serverErr = false;

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
        } else if (code == 429) {
            Serial.println("[Weather] 429 — OWM rate limit (server-side, not counted)");
            serverErr = true;
        } else if (code >= 500) {
            Serial.printf("[Weather] %d — OWM server error (not counted)\n", code);
            serverErr = true;
        } else {
            Serial.printf("[Weather] HTTP error %d\n", code);
        }
        http.end();
    } else {
        Serial.println("[Weather] http.begin() failed (check WiFi)");
    }

    if (!ok && !cfgErr && !serverErr) {
        weatherFails++;
        Serial.printf("[Weather] network fail %d / %d\n", weatherFails, WEATHER_FAIL_MAX);
        if (weatherFails >= WEATHER_FAIL_MAX) {
            Serial.printf("[Weather] %d consecutive failures → recovery\n", weatherFails);
            triggerRecovery();
        }
    }
}
