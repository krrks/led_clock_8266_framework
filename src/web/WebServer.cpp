// WebServer.cpp — HTTP server, REST API, WebSocket endpoints
#include "WebServer.h"
#include "LittleFS.h"
#include "config/ConfigManager.h"
#include "PinDefinitions.h"
#include "AppState.h"
#include <Updater.h>
#include <ArduinoJson.h>

WebServer webServer;

void WebServer::begin() {
    _server = new AsyncWebServer(80);

    _wsDashboard = new AsyncWebSocket("/ws/dashboard");
    _wsSerial    = new AsyncWebSocket("/ws/serial");

    _wsSerial->onEvent([](AsyncWebSocket*, AsyncWebSocketClient* c,
                          AwsEventType t, void*, uint8_t* d, size_t len) {
        if (t == WS_EVT_DATA) {
            for (size_t i = 0; i < len; i++) Serial.write(d[i]);
        }
    });

    _server->addHandler(_wsDashboard);
    _server->addHandler(_wsSerial);

    _setupRoutes();

    // Static files from LittleFS (fallback chain)
    _server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    _server->begin();
    Serial.println(F("[Web] HTTP server started on port 80"));
}

void WebServer::_setupRoutes() {
    using namespace std::placeholders;

    _server->on("/api/config", HTTP_GET,
                std::bind(&WebServer::_handleConfigGet, this, _1));

    _server->on("/api/config", HTTP_POST,
                [this](AsyncWebServerRequest* req) { _handleConfigPost(req); });

    _server->on("/api/pins", HTTP_GET,
                std::bind(&WebServer::_handlePinsGet, this, _1));

    _server->on("/api/status", HTTP_GET,
                std::bind(&WebServer::_handleStatusGet, this, _1));

    _server->on("/api/firmware", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            req->send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
            if (!Update.hasError()) {
                delay(200); ESP.restart();
            }
        },
        [this](AsyncWebServerRequest* req, String fn, size_t idx,
               uint8_t* d, size_t len, bool final) {
            _handleFirmwareUpload(req, fn, idx, d, len, final);
        });
}

// ── Config handlers ──────────────────────────────────────────────────
void WebServer::_handleConfigGet(AsyncWebServerRequest* req) {
    JsonDocument doc;
    auto& d = configManager.data;
    doc["projectName"]  = d.projectName;
    doc["brightness"]   = d.brightness;
    doc["brightDim"]    = d.brightDim;
    doc["brightMed"]    = d.brightMed;
    doc["brightBrt"]    = d.brightBrt;
    doc["use24h"]       = d.use24h;
    doc["rotation"]     = d.rotation;
    doc["flip"]         = d.flip;
    doc["scrollSpeed"]  = d.scrollSpeed;
    doc["timezone"]     = d.timezone;
    doc["wifiEnabled"]  = d.wifiEnabled;
    doc["wifiSSID"]     = d.wifiSSID;
    doc["wifiPassword"] = d.wifiPassword;
    doc["defaultWeather"]  = d.defaultWeather;
    doc["weatherApiKey"]   = d.weatherApiKey;
    doc["weatherCity"]     = d.weatherCity;
    doc["manualHour"]   = d.manualHour;
    doc["manualMinute"] = d.manualMinute;
    doc["manualDay"]    = d.manualDay;
    doc["manualMonth"]  = d.manualMonth;
    doc["manualYear"]   = d.manualYear;
    doc["manualWeekday"]= d.manualWeekday;
    doc["serialMonitor"]   = d.serialMonitorEnabled;
    doc["wirelessSerial"]  = d.wirelessSerialEnabled;
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

void WebServer::_handleConfigPost(AsyncWebServerRequest* req) {
    if (!req->hasParam("plain", true)) {
        req->send(400, "text/plain", "missing body");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, req->getParam("plain", true)->value());
    if (err) {
        req->send(400, "text/plain", "invalid JSON");
        return;
    }
    auto& d = configManager.data;
    if (doc.containsKey("projectName"))   strlcpy(d.projectName,  doc["projectName"],  sizeof(d.projectName));
    if (doc.containsKey("brightness"))    d.brightness   = doc["brightness"];
    if (doc.containsKey("brightDim"))     d.brightDim    = doc["brightDim"];
    if (doc.containsKey("brightMed"))     d.brightMed    = doc["brightMed"];
    if (doc.containsKey("brightBrt"))     d.brightBrt    = doc["brightBrt"];
    if (doc.containsKey("use24h"))        d.use24h       = doc["use24h"];
    if (doc.containsKey("rotation"))      d.rotation     = doc["rotation"];
    if (doc.containsKey("flip"))          d.flip         = doc["flip"];
    if (doc.containsKey("scrollSpeed"))   d.scrollSpeed  = doc["scrollSpeed"];
    if (doc.containsKey("timezone"))      strlcpy(d.timezone,         doc["timezone"],         sizeof(d.timezone));
    if (doc.containsKey("wifiEnabled"))   d.wifiEnabled  = doc["wifiEnabled"];
    if (doc.containsKey("wifiSSID"))      strlcpy(d.wifiSSID,         doc["wifiSSID"],         sizeof(d.wifiSSID));
    if (doc.containsKey("wifiPassword"))  strlcpy(d.wifiPassword,     doc["wifiPassword"],     sizeof(d.wifiPassword));
    if (doc.containsKey("defaultWeather")) d.defaultWeather = doc["defaultWeather"];
    if (doc.containsKey("weatherApiKey")) strlcpy(d.weatherApiKey,    doc["weatherApiKey"],    sizeof(d.weatherApiKey));
    if (doc.containsKey("weatherCity"))   strlcpy(d.weatherCity,      doc["weatherCity"],      sizeof(d.weatherCity));
    if (doc.containsKey("manualHour"))    d.manualHour   = doc["manualHour"];
    if (doc.containsKey("manualMinute"))  d.manualMinute = doc["manualMinute"];
    if (doc.containsKey("manualDay"))     d.manualDay    = doc["manualDay"];
    if (doc.containsKey("manualMonth"))   d.manualMonth  = doc["manualMonth"];
    if (doc.containsKey("manualYear"))    d.manualYear   = doc["manualYear"];
    if (doc.containsKey("manualWeekday")) d.manualWeekday= doc["manualWeekday"];
    if (doc.containsKey("serialMonitor")) d.serialMonitorEnabled  = doc["serialMonitor"];
    if (doc.containsKey("wirelessSerial"))d.wirelessSerialEnabled = doc["wirelessSerial"];

    configManager.save();
    req->send(200, "text/plain", "Saved");
}

// ── Pin info ─────────────────────────────────────────────────────────
void WebServer::_handlePinsGet(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc["pins"].to<JsonArray>();
    for (size_t i = 0; i < PIN_TABLE_COUNT; i++) {
        JsonObject p = arr.add<JsonObject>();
        p["gpio"]        = pinTable[i].gpio;
        p["name"]        = pinTable[i].name;
        p["description"] = pinTable[i].description;
        p["mode"]        = pinTable[i].mode;
    }
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

// ── System status ────────────────────────────────────────────────────
void WebServer::_handleStatusGet(AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["heap"]       = ESP.getFreeHeap();
    doc["uptime"]     = millis() / 1000;
    doc["flashSize"]  = ESP.getFlashChipRealSize();
    doc["sketchSize"] = ESP.getSketchSize();
    doc["reset"]      = ESP.getResetReason();
    doc["ntpSynced"]  = ntpSynced;
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

// ── Firmware upload ──────────────────────────────────────────────────
void WebServer::_handleFirmwareUpload(AsyncWebServerRequest*, String filename,
                                       size_t index, uint8_t* data, size_t len,
                                       bool final) {
    if (!index) {
        uint32_t maxSize = (uint32_t)ESP.getFreeSketchSpace() & 0xFFFFF;
        Serial.printf("[Web] OTA begin: %s max=%uB\n", filename.c_str(), maxSize);
        Update.begin(maxSize, U_FLASH);
    }
    if (len) Update.write(data, len);
    if (final) {
        Update.end(true);
        Serial.println(F("[Web] OTA done"));
    }
}
