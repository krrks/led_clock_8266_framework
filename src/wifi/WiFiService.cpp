// WiFiService.cpp — WiFi STA+AP management (non-blocking)
#include "WiFiService.h"

WiFiService WiFiManager;  // global alias for framework compatibility

void WiFiService::begin(const char* apName, unsigned long timeoutMs) {
    _started = true;
    _apMode  = false;
    _apName  = (apName && apName[0]) ? apName : "ESP8266-RECOVERY";
    _connectTimeout = timeoutMs;
    _tConnectStart  = millis();
    _announced = false;

    if (_ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        Serial.printf("[WiFi] STA connecting to %s (non-blocking)\n", _ssid.c_str());
        WiFi.begin(_ssid.c_str(), _pass.c_str());
    } else {
        Serial.println(F("[WiFi] no credentials — AP mode"));
        _enterAP();
    }
}

void WiFiService::loop() {
    if (!_started) return;

    if (_apMode) {
        _dnsServer.processNextRequest();
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (!_announced) {
            _announced = true;
            Serial.printf("[WiFi] STA connected  IP=%s\n", WiFi.localIP().toString().c_str());
        }
        return;
    }

    if (millis() - _tConnectStart >= _connectTimeout) {
        Serial.println(F("[WiFi] connect timeout → AP"));
        _enterAP();
    }
}

void WiFiService::_enterAP() {
    _apMode = true;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_apName.c_str());
    _dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.printf("[WiFi] AP mode: %s  IP=%s\n",
                  _apName.c_str(), WiFi.softAPIP().toString().c_str());
}

void WiFiService::setCredentials(const char* ssid, const char* password) {
    _ssid = ssid;
    _pass = password;
}
