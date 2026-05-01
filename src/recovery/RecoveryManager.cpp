// RecoveryManager.cpp — boot check, web server, serial monitor, OTA
#include "RecoveryManager.h"
#include "RecoveryHTML.h"
#include "LittleFS.h"
#include <ESP8266WiFi.h>
#include <Updater.h>
#include <ArduinoJson.h>
#include <user_interface.h>

// ─── Serial Redirect ────────────────────────────────────────────────────

SerialRedirect::SerialRedirect(HardwareSerial& hw, bool wired, bool wireless)
    : _hw(hw), _wired(wired), _wireless(wireless), _head(0), _tail(0) {
    memset((void*)_buf, 0, BUF_SZ);
}

void SerialRedirect::setWiredEnabled(bool en)   { _wired = en; }
void SerialRedirect::setWirelessEnabled(bool en) { _wireless = en; }

size_t SerialRedirect::write(uint8_t c) {
    if (_wired) _hw.write(c);
    if (_wireless) {
        int next = (_head + 1) % BUF_SZ;
        if (next != _tail) { _buf[_head] = c; _head = next; }
    }
    return 1;
}

size_t SerialRedirect::write(const uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) write(buf[i]);
    return len;
}

size_t SerialRedirect::printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) return write((const uint8_t*)buf, (size_t)n);
    return 0;
}

int SerialRedirect::available() {
    return (_head + BUF_SZ - _tail) % BUF_SZ;
}

int SerialRedirect::read() {
    if (_tail == _head) return -1;
    uint8_t c = _buf[_tail];
    _tail = (_tail + 1) % BUF_SZ;
    return c;
}

int SerialRedirect::peek() {
    if (_tail == _head) return -1;
    return _buf[_tail];
}

void SerialRedirect::flush() {
    _hw.flush();
}

SerialRedirect SerialOut(Serial, true, false);

// ─── Recovery Manager singleton ─────────────────────────────────────────

RecoveryManager& RecoveryManager::get() {
    static RecoveryManager inst;
    return inst;
}

// ─── begin() — Boot check ───────────────────────────────────────────────

void RecoveryManager::begin() {
    _active = false;

    // 1. Check reset reason
    String rsn = ESP.getResetReason();
    bool crashReset = (rsn == "Exception" || rsn.indexOf("Watchdog") >= 0);

    // 2. Check RTC memory flag
    RTCData rtc = {}; bool rtcFlag = false;
    if (ESP.rtcUserMemoryRead(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc))) {
        if (rtc.magic == RTC_MAGIC && rtc.enterRecovery == 1) {
            rtcFlag = true;
            rtc.enterRecovery = 0;
            ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
        }
    }

    // 3. Boot window — watch for button hold
    bool bootHold = false;
    if (triggerPin > 0) {
        pinMode(triggerPin, INPUT_PULLUP);
        Serial.printf("[Recovery] boot window %ums — hold GPIO%d\n", bootWindowMs, triggerPin);
        unsigned long start = millis();
        while (millis() - start < bootWindowMs) {
            if (digitalRead(triggerPin) == LOW) {
                unsigned long pressStart = millis();
                while (digitalRead(triggerPin) == LOW) {
                    if (millis() - pressStart >= 3000) { bootHold = true; break; }
                    delay(10);
                }
                if (bootHold) break;
            }
            delay(10);
            yield();
        }
    }

    if (!crashReset && !rtcFlag && !bootHold) {
        Serial.printf("[Recovery] normal boot: crash=%d rtc=%d btn=%d\n",
                      crashReset, rtcFlag, bootHold);
        return;
    }

    _active = true;
    Serial.printf("[Recovery] ENTERING: crash=%d rtc=%d btn=%d\n",
                  crashReset, rtcFlag, bootHold);

    // Try STA with saved credentials
    if (staSSID.length() > 0) {
        _startSTA();
    }

    // If STA failed, start AP
    if (!_apMode && WiFi.status() != WL_CONNECTED) {
        _startAP();
    }

    _startWebServer();
    SerialOut.setWiredEnabled(serialMonitorEnabled);
    SerialOut.setWirelessEnabled(wirelessSerialEnabled);
}

void RecoveryManager::_startSTA() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(staSSID.c_str(), staPassword.c_str());
    Serial.printf("[Recovery] STA connecting: %s\n", staSSID.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(200); Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[Recovery] STA OK  IP=%s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println(F("[Recovery] STA failed — falling back to AP"));
    }
}

void RecoveryManager::_startAP() {
    _apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID.c_str(), apPassword.length() > 0 ? apPassword.c_str() : nullptr);
    _dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.printf("[Recovery] AP: %s  IP=%s\n",
                  apSSID.c_str(), WiFi.softAPIP().toString().c_str());
}

void RecoveryManager::_startWebServer() {
    _server = new AsyncWebServer(webPort);

    // ── Firmware upload ───────────────────────────────────────────────
    _server->on("/firmware", HTTP_POST,
        [this](AsyncWebServerRequest* req) {
            Serial.printf("[Recovery] POST /firmware from %s\n",
                          req->client()->remoteIP().toString().c_str());
            req->send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
            if (!Update.hasError()) {
                Serial.println("[Recovery] firmware OK — rebooting");
                delay(200); ESP.restart();
            }
        },
        [this](AsyncWebServerRequest* req, String filename, size_t index,
               uint8_t* data, size_t len, bool final) {
            _handleUpload(req, filename, index, data, len, final);
        });

    // ── File list ─────────────────────────────────────────────────────
    _server->on("/api/files", HTTP_GET, [](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] GET /api/files from %s\n",
                      req->client()->remoteIP().toString().c_str());
        JsonDocument doc;
        JsonArray arr = doc["files"].to<JsonArray>();
        Dir dir = LittleFS.openDir("/");
        while (dir.next()) {
            JsonObject f = arr.add<JsonObject>();
            f["name"] = dir.fileName();
            f["size"] = dir.fileSize();
        }
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    // ── File delete ───────────────────────────────────────────────────
    _server->on("/api/files/delete", HTTP_POST, [](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] POST /api/files/delete from %s\n",
                      req->client()->remoteIP().toString().c_str());
        if (req->hasParam("name", true)) {
            String name = req->getParam("name", true)->value();
            if (name.startsWith("/")) name = "/" + name;
            else name = "/" + name;
            LittleFS.remove(name);
            req->send(200, "text/plain", "OK");
        } else {
            req->send(400, "text/plain", "missing name");
        }
    });

    // ── Serial monitor toggle ──────────────────────────────────────────
    _server->on("/api/serial", HTTP_GET, [this](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] GET /api/serial from %s\n",
                      req->client()->remoteIP().toString().c_str());
        JsonDocument doc;
        doc["wired"]    = serialMonitorEnabled;
        doc["wireless"] = wirelessSerialEnabled;
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    _server->on("/api/serial", HTTP_POST, [this](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] POST /api/serial from %s\n",
                      req->client()->remoteIP().toString().c_str());
        if (req->hasParam("wired", true))
            serialMonitorEnabled = (req->getParam("wired", true)->value() == "true");
        if (req->hasParam("wireless", true))
            wirelessSerialEnabled = (req->getParam("wireless", true)->value() == "true");
        SerialOut.setWiredEnabled(serialMonitorEnabled);
        SerialOut.setWirelessEnabled(wirelessSerialEnabled);
        Serial.printf("[Recovery] serial wired=%d wireless=%d\n",
                      serialMonitorEnabled, wirelessSerialEnabled);
        req->send(200, "text/plain", "OK");
    });

    // ── WiFi settings ───────────────────────────────────────────────────
    _server->on("/api/wifi", HTTP_GET, [this](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] GET /api/wifi from %s\n",
                      req->client()->remoteIP().toString().c_str());
        JsonDocument doc;
        doc["ssid"]     = staSSID;
        doc["password"] = staPassword;
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    _server->on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] POST /api/wifi from %s\n",
                      req->client()->remoteIP().toString().c_str());
        if (req->hasParam("ssid", true))
            staSSID = req->getParam("ssid", true)->value();
        if (req->hasParam("password", true))
            staPassword = req->getParam("password", true)->value();
        _saveWiFiConfig();
        req->send(200, "text/plain", "WiFi saved");
    });

    _server->on("/api/wifi", HTTP_DELETE, [this](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] DELETE /api/wifi from %s\n",
                      req->client()->remoteIP().toString().c_str());
        staSSID = "";
        staPassword = "";
        _saveWiFiConfig();
        req->send(200, "text/plain", "WiFi cleared");
    });

    // ── Serial WebSocket ──────────────────────────────────────────────
    _wsSerial = new AsyncWebSocket("/ws/serial");
    _wsSerial->onEvent([this](AsyncWebSocket* s, AsyncWebSocketClient* c,
                              AwsEventType t, void*, uint8_t* d, size_t len) {
        switch (t) {
        case WS_EVT_CONNECT:
            Serial.printf("[Recovery] WS /ws/serial connect from %s\n",
                          c->remoteIP().toString().c_str());
            wirelessSerialEnabled = true;
            SerialOut.setWirelessEnabled(true);
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("[Recovery] WS /ws/serial disconnect from %s\n",
                          c->remoteIP().toString().c_str());
            break;
        case WS_EVT_DATA: {
            String cmd; cmd.reserve(len + 1);
            for (size_t i = 0; i < len; i++) cmd += (char)d[i];
            cmd.trim();
            if (cmd.length() == 0) break;
            cmd.toLowerCase();

            // Command table — add new commands here
            Cmd commands[] = {
                {"help",   &RecoveryManager::_wsHelp},
                {"?",      &RecoveryManager::_wsHelp},
                {"status", &RecoveryManager::_wsStatus},
                {"stats",  &RecoveryManager::_wsStatus},
                {"heap",   &RecoveryManager::_wsHeap},
                {"reboot", &RecoveryManager::_wsReboot},
                {"wifi",   &RecoveryManager::_wsWifi},
                {"clear",  &RecoveryManager::_wsClear},
                {"ls",     &RecoveryManager::_wsLs},
                {"dir",    &RecoveryManager::_wsLs},
                {"info",   &RecoveryManager::_wsInfo},
                {"sys",    &RecoveryManager::_wsInfo},
                {"scan",   &RecoveryManager::_wsScan},
                {"df",     &RecoveryManager::_wsDf},
                {"flash",  &RecoveryManager::_wsDf},
                {"reset",  &RecoveryManager::_wsReset},
                {"factory",&RecoveryManager::_wsReset},
            };

            bool found = false;
            for (auto& x : commands) {
                if (cmd == x.name) {
                    (this->*x.fn)(c);
                    found = true;
                    break;
                }
            }
            if (!found) {
                if (serialMonitorEnabled) {
                    Serial.write(cmd.c_str(), cmd.length());
                    Serial.write("\r\n");
                }
                c->text("Unknown: " + cmd + " — type help\r\n");
            }
            break;
        }
        default: break;
        }
    });
    _server->addHandler(_wsSerial);

    // ── System info ───────────────────────────────────────────────────
    _server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] GET /api/status from %s\n",
                      req->client()->remoteIP().toString().c_str());
        JsonDocument doc;
        doc["heap"]     = ESP.getFreeHeap();
        doc["uptime"]   = millis() / 1000;
        doc["reset"]    = ESP.getResetReason();
        doc["flashSize"] = ESP.getFlashChipRealSize();
        doc["sketchSize"]= ESP.getSketchSize();
        String json;
        serializeJson(doc, json);
        req->send(200, "application/json", json);
    });

    // ── Mode detection for AI / external tools ──────────────────────────
    _server->on("/api/mode", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "application/json", "{\"mode\":\"recovery\"}");
    });

    // ── Exit recovery / reboot to normal ───────────────────────────────
    _server->on("/api/exit", HTTP_POST, [this](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] POST /api/exit from %s\n",
                      req->client()->remoteIP().toString().c_str());
        RTCData rtc = { RTC_MAGIC, 0 };
        if (ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc))) {
            Serial.println(F("[Recovery] RTC cleared — rebooting to normal"));
            req->send(200, "text/plain", "OK");
        } else {
            Serial.println(F("[Recovery] RTC write failed"));
            req->send(500, "text/plain", "RTC write failed");
            return;
        }
        delay(200); ESP.restart();
    });

    // ── Root — serve recovery SPA ─────────────────────────────────────
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        Serial.printf("[Recovery] GET / from %s\n",
                      req->client()->remoteIP().toString().c_str());
        req->send(200, "text/html", _getRecoveryHTML());
    });

    _server->begin();
    Serial.printf("[Recovery] web server on port %u\n", webPort);
}

void RecoveryManager::_handleUpload(AsyncWebServerRequest*, String filename,
                                     size_t index, uint8_t* data, size_t len,
                                     bool final) {
    if (!index) {
        uint32_t maxSize = (uint32_t)ESP.getFreeSketchSpace() & 0xFFFFF;
        Serial.printf("[Recovery] OTA begin: %s max=%uB\n",
                      filename.c_str(), maxSize);
        Update.begin(maxSize, U_FLASH);
    }
    if (len) Update.write(data, len);
    if (final) {
        Update.end(true);
        Serial.println(F("[Recovery] OTA done"));
    }
}

void RecoveryManager::_saveWiFiConfig() {
    // Read existing config.json, update WiFi fields, write back
    File f = LittleFS.open("/config.json", "r");
    JsonDocument doc;
    if (f) {
        deserializeJson(doc, f);
        f.close();
    }
    doc["wifiSSID"]     = staSSID;
    doc["wifiPassword"] = staPassword;
    f = LittleFS.open("/config.json", "w");
    if (f) {
        serializeJson(doc, f);
        f.close();
        Serial.printf("[Recovery] WiFi config saved: %s\n",
                      staSSID.length() ? staSSID.c_str() : "(cleared)");
    } else {
        Serial.println(F("[Recovery] ERROR: cannot write config.json"));
    }
}

// ─── Serial command handlers ─────────────────────────────────────────────

void RecoveryManager::_wsHelp(AsyncWebSocketClient* c) {
    c->text(
        "help/?      status/stats  heap      reboot\r\n"
        "wifi        clear         ls/dir    info/sys\r\n"
        "scan        df/flash      reset/factory\r\n"
    );
}

void RecoveryManager::_wsStatus(AsyncWebSocketClient* c) {
    String r;
    r += "Heap: " + String(ESP.getFreeHeap()) + " B\r\n";
    r += "Uptime: " + String(millis() / 1000) + " s\r\n";
    r += "Reset: " + ESP.getResetReason() + "\r\n";
    r += "Mode: " + String(_apMode ? "AP" : "STA") + "\r\n";
    if (_apMode)
        r += "AP IP: " + WiFi.softAPIP().toString() + "\r\n";
    else
        r += "IP: " + WiFi.localIP().toString() + "\r\n";
    r += "Serial wired=" + String(serialMonitorEnabled)
       + " wireless=" + String(wirelessSerialEnabled) + "\r\n";
    c->text(r);
}

void RecoveryManager::_wsHeap(AsyncWebSocketClient* c) {
    c->text(String("Free heap: ") + ESP.getFreeHeap() + " B\r\n");
}

void RecoveryManager::_wsReboot(AsyncWebSocketClient* c) {
    c->text("Rebooting...\r\n");
    delay(100); ESP.restart();
}

void RecoveryManager::_wsWifi(AsyncWebSocketClient* c) {
    String r;
    r += "STA SSID: " + (staSSID.length() ? staSSID : "(none)") + "\r\n";
    if (_apMode)
        r += "AP SSID: " + apSSID + "\r\n"
           + "AP IP: " + WiFi.softAPIP().toString() + "\r\n";
    else
        r += "IP: " + WiFi.localIP().toString() + "\r\n";
    c->text(r);
}

void RecoveryManager::_wsClear(AsyncWebSocketClient* c) {
    c->text("\033[2J\033[H");
}

void RecoveryManager::_wsLs(AsyncWebSocketClient* c) {
    Dir dir = LittleFS.openDir("/");
    String r;
    int n = 0;
    while (dir.next()) {
        r += dir.fileName() + "  " + String(dir.fileSize()) + " B\r\n";
        n++;
    }
    c->text(n ? r : "(empty)\r\n");
}

void RecoveryManager::_wsInfo(AsyncWebSocketClient* c) {
    String r;
    r += "Chip ID:   " + String(ESP.getChipId(), HEX) + "\r\n";
    r += "CPU Freq:  " + String(ESP.getCpuFreqMHz()) + " MHz\r\n";
    r += "SDK:       " + String(ESP.getSdkVersion()) + "\r\n";
    r += "Flash:     " + String(ESP.getFlashChipRealSize()) + " B\r\n";
    r += "Sketch:    " + String(ESP.getSketchSize()) + " B\r\n";
    r += "Free heap: " + String(ESP.getFreeHeap()) + " B\r\n";
    c->text(r);
}

void RecoveryManager::_wsScan(AsyncWebSocketClient* c) {
    c->text("Scanning WiFi...\r\n");
    int n = WiFi.scanNetworks();
    if (n <= 0) {
        c->text("No networks found.\r\n");
        return;
    }
    String r;
    for (int i = 0; i < n; i++) {
        r += String(i + 1) + ". " + WiFi.SSID(i) + "  ("
           + String(WiFi.RSSI(i)) + " dBm)  "
           + (WiFi.encryptionType(i) == ENC_TYPE_NONE ? "OPEN" : "***") + "\r\n";
    }
    c->text(r);
}

void RecoveryManager::_wsDf(AsyncWebSocketClient* c) {
    FSInfo fs;
    LittleFS.info(fs);
    String r;
    r += "Sketch:  " + String(ESP.getSketchSize()) + " B\r\n";
    r += "Flash:   " + String(ESP.getFlashChipRealSize()) + " B total\r\n";
    r += "LittleFS: " + String(fs.totalBytes) + " B total, "
       + String(fs.usedBytes) + " B used ("
       + String(fs.usedBytes * 100 / fs.totalBytes) + "%)\r\n";
    r += "Free:    " + String(ESP.getFreeHeap()) + " B heap, "
       + String(ESP.getFlashChipRealSize() - ESP.getSketchSize() - fs.totalBytes) + " B flash\r\n";
    c->text(r);
}

void RecoveryManager::_wsReset(AsyncWebSocketClient* c) {
    c->text("Factory reset — clearing config & rebooting...\r\n");
    LittleFS.remove("/config.json");
    RTCData rtc = { RTC_MAGIC, 0 };
    ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
    delay(200); ESP.restart();
}

void RecoveryManager::_broadcastSerial() {
    if (!_wsSerial || _wsSerial->count() == 0) return;
    while (SerialOut.available() > 0) {
        char c = (char)SerialOut.read();
        _wsSerial->textAll(String(c));
    }
}

// ─── trigger() — software entry ────────────────────────────────────────

void RecoveryManager::trigger() {
    Serial.println(F("[Recovery] software trigger → RTC flag → reboot"));
    RTCData rtc = { RTC_MAGIC, 1 };
    ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
    delay(50);
    ESP.restart();
}

// ─── loop() ────────────────────────────────────────────────────────────

void RecoveryManager::loop() {
    if (!_active) return;
    if (_apMode) _dnsServer.processNextRequest();
    _broadcastSerial();
}

// Recovery web UI — see src/recovery/RecoveryHTML.h

String RecoveryManager::_getRecoveryHTML() {
    return String(FPSTR(RECOVERY_HTML));
}
