// WebServer.cpp — HTTP server, REST API, WebSocket endpoints
#include "WebServer.h"
#include "LittleFS.h"
#include <ESP8266WiFi.h>
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

    _wsDashboard->onEvent([](AsyncWebSocket*, AsyncWebSocketClient* c,
                             AwsEventType t, void*, uint8_t*, size_t) {
        switch (t) {
        case WS_EVT_CONNECT:
            Serial.printf("[Web] WS /ws/dashboard connect from %s\n",
                          c->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[Web] WS /ws/dashboard disconnect from %s\n",
                          c->remoteIP().toString().c_str());
            break;
        default: break;
        }
    });

    _wsSerial->onEvent([](AsyncWebSocket*, AsyncWebSocketClient* c,
                          AwsEventType t, void*, uint8_t* d, size_t len) {
        switch (t) {
        case WS_EVT_CONNECT:
            Serial.printf("[Web] WS /ws/serial connect from %s\n",
                          c->remoteIP().toString().c_str());
            configManager.data.wirelessSerialEnabled = true;
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[Web] WS /ws/serial disconnect from %s\n",
                          c->remoteIP().toString().c_str());
            break;
        case WS_EVT_DATA: {
            String cmd; cmd.reserve(len + 1);
            for (size_t i = 0; i < len; i++) cmd += (char)d[i];
            cmd.trim();
            if (cmd.length() == 0) break;
            cmd.toLowerCase();
            if (cmd == "help" || cmd == "?") {
                c->text(
                    "help/?      status/stats  heap      reboot\r\n"
                    "wifi        clear         ls/dir    info/sys\r\n"
                );
            } else if (cmd == "status" || cmd == "stats") {
                String r;
                r += "Heap: " + String(ESP.getFreeHeap()) + " B\r\n";
                r += "Uptime: " + String(millis() / 1000) + " s\r\n";
                r += "Reset: " + ESP.getResetReason() + "\r\n";
                r += "NTP: " + String(ntpSynced ? "synced" : "manual") + "\r\n";
                r += "WiFi: " + String(WiFi.status() == WL_CONNECTED ? "connected" : "offline") + "\r\n";
                if (WiFi.status() == WL_CONNECTED)
                    r += "IP: " + WiFi.localIP().toString() + "\r\n";
                c->text(r);
            } else if (cmd == "heap") {
                c->text(String("Free heap: ") + ESP.getFreeHeap() + " B\r\n");
            } else if (cmd == "reboot") {
                c->text("Rebooting...\r\n");
                delay(100); ESP.restart();
            } else if (cmd == "wifi") {
                String r;
                r += "Status: " + String(WiFi.status() == WL_CONNECTED ? "connected" : "offline") + "\r\n";
                r += "Mode: " + String(WiFi.getMode() == WIFI_AP ? "AP" : (WiFi.getMode() == WIFI_STA ? "STA" : "other")) + "\r\n";
                r += "IP: " + WiFi.localIP().toString() + "\r\n";
                r += "AP IP: " + WiFi.softAPIP().toString() + "\r\n";
                c->text(r);
            } else if (cmd == "clear") {
                c->text("\033[2J\033[H");
            } else if (cmd == "ls" || cmd == "dir") {
                Dir dir = LittleFS.openDir("/");
                String r; int n = 0;
                while (dir.next()) {
                    r += dir.fileName() + "  " + String(dir.fileSize()) + " B\r\n";
                    n++;
                }
                c->text(n ? r : "(empty)\r\n");
            } else if (cmd == "info" || cmd == "sys") {
                String r;
                r += "Chip: " + String(ESP.getChipId(), HEX) + "\r\n";
                r += "CPU: " + String(ESP.getCpuFreqMHz()) + " MHz\r\n";
                r += "SDK: " + String(ESP.getSdkVersion()) + "\r\n";
                r += "Flash: " + String(ESP.getFlashChipRealSize()) + " B\r\n";
                r += "Sketch: " + String(ESP.getSketchSize()) + " B\r\n";
                r += "Free heap: " + String(ESP.getFreeHeap()) + " B\r\n";
                c->text(r);
            } else {
                Serial.write(cmd.c_str(), cmd.length());
                Serial.write("\r\n");
                c->text("Unknown: " + cmd + " — type help\r\n");
            }
            break;
        }
        default: break;
        }
    });

    _server->addHandler(_wsDashboard);
    _server->addHandler(_wsSerial);

    // Log every HTTP request to serial (canHandle → false, never consumes)
    struct ReqLogger : public AsyncWebHandler {
        bool canHandle(AsyncWebServerRequest* req) {
            const char* m = req->method() == HTTP_POST ? "POST"
                          : req->method() == HTTP_GET  ? "GET"  : "...";
            Serial.printf("[Web] %s %s from %s\n",
                          m, req->url().c_str(),
                          req->client()->remoteIP().toString().c_str());
            return false;
        }
        void handleRequest(AsyncWebServerRequest*) {}
        bool isRequestHandlerTrivial() { return true; }
    };
    _server->addHandler(new ReqLogger());

    _setupRoutes();

    // Static files from LittleFS (fallback chain)
    _server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // Captive portal + fallback when LittleFS files are missing
    _server->onNotFound([](AsyncWebServerRequest* req) {
        if (req->method() == HTTP_GET && req->url() != "/") {
            req->redirect("/");
        } else if (req->method() == HTTP_GET && req->url() == "/") {
            req->send(200, "text/html", F(
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' "
"content='width=device-width,initial-scale=1'><title>LED Clock</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font:14px monospace;background:#1a1a2e;color:#e0e0e0;max-width:640px;margin:40px auto;padding:20px}"
"h1{color:#ff6600;text-align:center;margin-bottom:16px}"
".card{background:#16213e;padding:16px;border-radius:6px;margin:12px 0}"
".card h2{color:#ff8833;margin-bottom:8px;font-size:16px}"
"a{color:#ff8833}"
"code{background:#0f3460;padding:2px 6px;border-radius:3px}"
"pre{background:#000;color:#0f0;padding:12px;border-radius:4px;overflow-x:auto;margin:8px 0}"
"</style></head><body>"
"<h1>LED Matrix Clock</h1>"
"<div class='card'>"
"<h2>Web UI files not on device</h2>"
"<p>Upload the <code>data/</code> directory via PlatformIO:</p>"
"<pre>pio run --target uploadfs</pre>"
"<p>Or access the <a href='/api/status'>REST API</a> directly.</p>"
"</div>"
"</body></html>"
            ));
        } else {
            req->send(404, "text/plain", "Not found");
        }
    });

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

    _server->on("/api/mode", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json",
                  appMode == AM_RECOVERY ? "{\"mode\":\"recovery\"}" : "{\"mode\":\"normal\"}");
    });

    _server->on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* req) {
        Serial.printf("[Web] POST /api/reboot from %s\n",
                      req->client()->remoteIP().toString().c_str());
        req->send(200, "text/plain", "rebooting");
        delay(200); ESP.restart();
    });

    _server->on("/api/serial", HTTP_GET, [](AsyncWebServerRequest* req) {
        JsonDocument doc;
        doc["wired"]    = configManager.data.serialMonitorEnabled;
        doc["wireless"] = configManager.data.wirelessSerialEnabled;
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    _server->on("/api/serial", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (req->hasParam("wired", true))
            configManager.data.serialMonitorEnabled = (req->getParam("wired", true)->value() == "true");
        if (req->hasParam("wireless", true))
            configManager.data.wirelessSerialEnabled = (req->getParam("wireless", true)->value() == "true");
        configManager.save();
        Serial.printf("[Web] serial wired=%d wireless=%d\n",
                      configManager.data.serialMonitorEnabled,
                      configManager.data.wirelessSerialEnabled);
        req->send(200, "text/plain", "OK");
    });

    _server->on("/api/command", HTTP_GET, [](AsyncWebServerRequest* req) {
        if (!req->hasParam("cmd")) {
            req->send(400, "text/plain", "missing cmd param");
            return;
        }
        String cmd = req->getParam("cmd")->value();
        cmd.toLowerCase();
        Serial.printf("[Web] GET /api/command?cmd=%s from %s\n",
                      cmd.c_str(), req->client()->remoteIP().toString().c_str());
        String r;
        if (cmd == "status" || cmd == "stats") {
            r += "heap=" + String(ESP.getFreeHeap()) + "\n";
            r += "uptime=" + String(millis() / 1000) + "\n";
            r += "reset=" + ESP.getResetReason() + "\n";
            r += "ntp=" + String(ntpSynced ? "1" : "0") + "\n";
            r += "wifi=" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\n";
            if (WiFi.status() == WL_CONNECTED)
                r += "ip=" + WiFi.localIP().toString() + "\n";
            r += "sketch=" + String(ESP.getSketchSize()) + "\n";
        } else if (cmd == "heap") {
            r = String(ESP.getFreeHeap());
        } else if (cmd == "reboot") {
            req->send(200, "text/plain", "rebooting");
            delay(200); ESP.restart();
            return;
        } else if (cmd == "wifi") {
            r += "status=" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\n";
            r += "mode=" + String(WiFi.getMode() == WIFI_AP ? "AP" : "STA") + "\n";
            r += "ip=" + WiFi.localIP().toString() + "\n";
            r += "ap_ip=" + WiFi.softAPIP().toString() + "\n";
        } else {
            r = "unknown cmd: " + cmd;
        }
        req->send(200, "text/plain", r);
    });

    _server->on("/api/firmware", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            Serial.printf("[Web] POST /api/firmware from %s\n",
                          req->client()->remoteIP().toString().c_str());
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
    Serial.printf("[Web] GET /api/config from %s\n",
                  req->client()->remoteIP().toString().c_str());
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
    Serial.printf("[Web] POST /api/config from %s\n",
                  req->client()->remoteIP().toString().c_str());
    auto& d = configManager.data;

    // Read each config field from POST params (urlencoded form data)
    #define GET_PARAM(name, field, conv) \
        if (req->hasParam(name, true)) field = req->getParam(name, true)->value().conv
    #define GET_STR(name, field, sz) \
        if (req->hasParam(name, true)) strlcpy(field, req->getParam(name, true)->value().c_str(), sz)

    GET_STR("projectName",   d.projectName,   sizeof(d.projectName));
    GET_PARAM("brightness",  d.brightness,    toInt());
    GET_PARAM("brightDim",   d.brightDim,     toInt());
    GET_PARAM("brightMed",   d.brightMed,     toInt());
    GET_PARAM("brightBrt",   d.brightBrt,     toInt());
    GET_PARAM("use24h",      d.use24h,        equalsIgnoreCase("true"));
    GET_PARAM("rotation",    d.rotation,      toInt());
    GET_PARAM("flip",        d.flip,          toInt());
    GET_PARAM("scrollSpeed", d.scrollSpeed,   toInt());
    GET_STR("timezone",      d.timezone,      sizeof(d.timezone));
    GET_PARAM("wifiEnabled", d.wifiEnabled,   equalsIgnoreCase("true"));
    GET_STR("wifiSSID",      d.wifiSSID,      sizeof(d.wifiSSID));
    GET_STR("wifiPassword",  d.wifiPassword,  sizeof(d.wifiPassword));
    GET_PARAM("defaultWeather", d.defaultWeather, equalsIgnoreCase("true"));
    GET_STR("weatherApiKey", d.weatherApiKey, sizeof(d.weatherApiKey));
    GET_STR("weatherCity",   d.weatherCity,   sizeof(d.weatherCity));
    GET_PARAM("manualHour",   d.manualHour,   toInt());
    GET_PARAM("manualMinute", d.manualMinute, toInt());
    GET_PARAM("manualDay",    d.manualDay,    toInt());
    GET_PARAM("manualMonth",  d.manualMonth,  toInt());
    GET_PARAM("manualYear",   d.manualYear,   toInt());
    GET_PARAM("manualWeekday",d.manualWeekday,toInt());
    GET_PARAM("serialMonitor", d.serialMonitorEnabled, equalsIgnoreCase("true"));
    GET_PARAM("wirelessSerial",d.wirelessSerialEnabled,equalsIgnoreCase("true"));

    #undef GET_PARAM
    #undef GET_STR

    configManager.save();
    Serial.println(F("[Web] config saved"));
    req->send(200, "text/plain", "Saved");
}

// ── Pin info ─────────────────────────────────────────────────────────
void WebServer::_handlePinsGet(AsyncWebServerRequest* req) {
    Serial.printf("[Web] GET /api/pins from %s\n",
                  req->client()->remoteIP().toString().c_str());
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
    Serial.printf("[Web] GET /api/status from %s\n",
                  req->client()->remoteIP().toString().c_str());
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
