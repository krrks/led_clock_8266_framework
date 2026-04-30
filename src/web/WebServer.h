#pragma once
// WebServer — ESP8266 web server on port 80
// Serves static files from LittleFS + REST API + WebSocket endpoints.
// Replaces maakbaas/esp8266-iot-framework webServer (React SPA).

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class WebServer {
public:
    void begin();
    void loop() {}  // Async — no polling needed

    AsyncWebSocket& wsDashboard() { return *_wsDashboard; }
    AsyncWebSocket& wsSerial()    { return *_wsSerial; }

private:
    AsyncWebServer* _server = nullptr;
    AsyncWebSocket* _wsDashboard = nullptr;
    AsyncWebSocket* _wsSerial = nullptr;

    void _setupRoutes();
    void _handleConfigGet(AsyncWebServerRequest* req);
    void _handleConfigPost(AsyncWebServerRequest* req);
    void _handlePinsGet(AsyncWebServerRequest* req);
    void _handleStatusGet(AsyncWebServerRequest* req);
    void _handleFirmwareUpload(AsyncWebServerRequest* req,
                               String filename, size_t index,
                               uint8_t* data, size_t len, bool final);
};

extern WebServer webServer;
