// Dashboard.cpp — WebSocket dashboard push
#include "Dashboard.h"
#include <ArduinoJson.h>

Dashboard dash;

void Dashboard::begin(AsyncWebSocket* ws, unsigned long intervalMs) {
    _ws = ws;
    _interval = intervalMs;
    _tLast = millis();
}

void Dashboard::loop() {
    if (!_ws || _ws->count() == 0) return;
    unsigned long now = millis();
    if (now - _tLast < _interval) return;
    _tLast = now;
    _push();
}

void Dashboard::_push() {
    JsonDocument doc;
    doc["currentTime"]  = currentTime;
    doc["currentDate"]  = currentDate;
    doc["weekday"]      = weekday;
    doc["ntpSynced"]    = ntpSynced;
    doc["temperature"]  = temperature;
    doc["weatherCode"]  = weatherCode;
    doc["weatherDesc"]  = weatherDesc;
    doc["freeHeap"]     = freeHeap;
    doc["uptime"]       = uptime;
    doc["inRecovery"]   = inRecovery;
    String json;
    serializeJson(doc, json);
    _ws->textAll(json);
}
