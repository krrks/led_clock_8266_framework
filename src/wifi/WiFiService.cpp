// WiFiService.cpp — WiFi STA+AP management
#include "WiFiService.h"

WiFiService WiFiManager;  // global alias for framework compatibility

void WiFiService::begin(const char* apName, unsigned long timeoutMs) {
    _started = true;
    _apMode  = false;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    if (_ssid.length() > 0) {
        Serial.printf("[WiFi] STA connecting to %s\n", _ssid.c_str());
        WiFi.begin(_ssid.c_str(), _pass.c_str());

        if (timeoutMs > 0) {
            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
                delay(200);
                Serial.print(".");
            }
            Serial.println();
        } else {
            // Non-blocking — will connect in background; loop() handles DNS
        }
    } else {
        Serial.println(F("[WiFi] no credentials — skipping STA"));
    }

    if (WiFi.status() != WL_CONNECTED) {
        // Start AP fallback
        _apMode = true;
        WiFi.mode(WIFI_AP);
        String apSSID = apName;
        if (apSSID.length() == 0) apSSID = "ESP8266-RECOVERY";
        WiFi.softAP(apSSID.c_str());
        _dnsServer.start(53, "*", WiFi.softAPIP());
        Serial.printf("[WiFi] AP mode: %s  IP=%s\n",
                      apSSID.c_str(), WiFi.softAPIP().toString().c_str());
    } else {
        Serial.printf("[WiFi] STA connected  IP=%s\n", WiFi.localIP().toString().c_str());
    }
}

void WiFiService::loop() {
    if (!_started) return;
    if (_apMode) {
        _dnsServer.processNextRequest();
    }
}

void WiFiService::setCredentials(const char* ssid, const char* password) {
    _ssid = ssid;
    _pass = password;
}
