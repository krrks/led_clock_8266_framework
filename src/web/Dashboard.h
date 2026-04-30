#pragma once
// Dashboard — WebSocket-based live data push to web clients
// Replaces maakbaas/esp8266-iot-framework dashboard.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class Dashboard {
public:
    void begin(AsyncWebSocket* ws, unsigned long intervalMs = 5000);
    void loop();  // call in main loop — pushes data at interval

    // Data fields (set by main loop before calling loop())
    char    currentTime[16] = "--:--:--";
    char    currentDate[16] = "----";
    char    weekday[8]      = "---";
    bool    ntpSynced      = false;
    float   temperature    = 0.0f;
    int16_t weatherCode    = 0;
    char    weatherDesc[32]= "N/A";
    uint32_t freeHeap      = 0;
    uint32_t uptime        = 0;
    bool    inRecovery     = false;

private:
    AsyncWebSocket* _ws = nullptr;
    unsigned long   _interval = 5000;
    unsigned long   _tLast = 0;
    void _push();
};

extern Dashboard dash;
