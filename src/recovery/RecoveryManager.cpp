// RecoveryManager.cpp — boot check, web server, serial monitor, OTA
#include "RecoveryManager.h"
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

    // ── Serial WebSocket ──────────────────────────────────────────────
    _wsSerial = new AsyncWebSocket("/ws/serial");
    _wsSerial->onEvent([](AsyncWebSocket* s, AsyncWebSocketClient* c,
                          AwsEventType t, void*, uint8_t* d, size_t len) {
        if (t == WS_EVT_DATA) {
            // Send received web input to hardware Serial
            for (size_t i = 0; i < len; i++) Serial.write(d[i]);
        }
    });
    _server->addHandler(_wsSerial);

    // ── System info ───────────────────────────────────────────────────
    _server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
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

    // ── Exit recovery / reboot ────────────────────────────────────────
    _server->on("/api/exit", HTTP_POST, [this](AsyncWebServerRequest* req) {
        RTCData rtc = { RTC_MAGIC, 0 };
        ESP.rtcUserMemoryWrite(0, reinterpret_cast<uint32_t*>(&rtc), sizeof(rtc));
        req->send(200, "text/plain", "rebooting");
        delay(200); ESP.restart();
    });

    // ── Root — serve recovery SPA ─────────────────────────────────────
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
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

// ═══════════════════════════════════════════════════════════════════════
// Recovery web UI — embedded PROGMEM HTML/JS/CSS
// Single-page app with tabs: Firmware | Files | Serial Monitor
// ═══════════════════════════════════════════════════════════════════════

String RecoveryManager::_getRecoveryHTML() {
    return String(F(
"<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' "
"content='width=device-width,initial-scale=1'><title>Recovery</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font:14px monospace;background:#1a1a2e;color:#e0e0e0;max-width:680px;margin:0 auto;padding:10px}"
"h1{color:#ff6600;text-align:center;margin:10px 0}"
".tabs{display:flex;gap:2px;margin-bottom:10px}"
".tab{padding:8px 16px;background:#16213e;border:none;color:#e0e0e0;cursor:pointer;border-radius:4px 4px 0 0;flex:1;text-align:center}"
".tab.active{background:#ff6600;color:#000}"
".panel{display:none;background:#16213e;padding:12px;border-radius:0 4px 4px 4px}"
".panel.active{display:block}"
"input,button,select{width:100%;padding:8px;margin:6px 0;border:none;border-radius:3px;font:14px monospace}"
"input{background:#0f3460;color:#e0e0e0}"
"button{background:#ff6600;color:#000;cursor:pointer;font-weight:bold}"
"button:hover{background:#ff8833}"
"#serialTerm{background:#000;color:#0f0;height:300px;overflow-y:auto;padding:8px;margin:6px 0;border-radius:3px;white-space:pre-wrap;word-break:break-all}"
"#serialInput{background:#0f3460;color:#0f0}"
".status{color:#ff6600;margin:6px 0;min-height:20px}"
".progress{width:100%;height:10px;background:#0f3460;border-radius:5px;margin:6px 0;overflow:hidden}"
".progress div{height:100%;background:#ff6600;width:0%;transition:width .3s}"
".fileItem{display:flex;justify-content:space-between;align-items:center;padding:4px 0;border-bottom:1px solid #0f3460}"
".fileItem button{width:auto;padding:4px 10px;margin:0}"
".info{color:#888;font-size:12px}"
"</style></head><body>"
"<h1>ESP8266 Recovery</h1>"
"<div class='tabs'>"
"<button class='tab active' onclick='showTab(\"firmware\")'>Firmware</button>"
"<button class='tab' onclick='showTab(\"files\")'>Files</button>"
"<button class='tab' onclick='showTab(\"serial\")'>Serial Monitor</button>"
"</div>"
"<div id='firmware' class='panel active'>"
"<p>Select a compiled .bin file and upload to update firmware.</p>"
"<input type='file' id='fwFile' accept='.bin' onchange='uploadFirmware()'>"
"<div class='progress'><div id='fwProgress'></div></div>"
"<div id='fwStatus' class='status'></div>"
"<button onclick='exitRecovery()'>Reboot to Normal</button>"
"</div>"
"<div id='files' class='panel'>"
"<h3>LittleFS Files</h3>"
"<div id='fileList'></div>"
"<button onclick='loadFiles()'>Refresh</button>"
"</div>"
"<div id='serial' class='panel'>"
"<div id='serialTerm'></div>"
"<input id='serialInput' placeholder='Type command + Enter...' "
"onkeydown='if(event.key==\"Enter\"){sendSerial(this.value);this.value=\"\"}'>"
"<div><input type='checkbox' id='serialAuto' checked onchange='toggleAuto()'>"
"<label for='serialAuto'>Auto-scroll</label></div>"
"</div>"
"<script>"
"var ws;var activeTab='firmware';var autoScroll=true;"
"function showTab(t){"
"activeTab=t;"
"document.querySelectorAll('.tab').forEach(function(e){e.classList.remove('active')});"
"event.target.classList.add('active');"
"document.querySelectorAll('.panel').forEach(function(e){e.classList.remove('active')});"
"document.getElementById(t).classList.add('active');"
"if(t==='serial')connectWS();else{if(ws)ws.close()}"
"if(t==='files')loadFiles();"
"}"
"function uploadFirmware(){"
"var f=document.getElementById('fwFile').files[0];if(!f)return;"
"var x=new XMLHttpRequest();"
"x.upload.onprogress=function(e){"
"document.getElementById('fwProgress').style.width=Math.round(e.loaded/e.total*100)+'%';};"
"x.onload=function(){document.getElementById('fwStatus').textContent=x.responseText;"
"if(x.responseText=='OK')document.getElementById('fwStatus').textContent='Success — rebooting...';};"
"x.onerror=function(){document.getElementById('fwStatus').textContent='Upload error';};"
"x.open('POST','/firmware');x.send(f);"
"document.getElementById('fwStatus').textContent='Uploading...';"
"}"
"function loadFiles(){"
"fetch('/api/files').then(function(r){return r.json()}).then(function(d){"
"var h='';d.files.forEach(function(f){"
"h+='<div class=fileItem><span>'+f.name+' <span class=info>('+f.size+'B)</span></span>'"
"+'<button onclick=delFile(\\''+f.name+'\\')>Delete</button></div>';});"
"document.getElementById('fileList').innerHTML=h||'<p>No files</p>';});"
"}"
"function delFile(n){"
"var f=new FormData();f.append('name',n);"
"fetch('/api/files/delete',{method:'POST',body:f}).then(function(){loadFiles()});}"
"function connectWS(){"
"if(ws){ws.close()}"
"ws=new WebSocket('ws://'+location.host+'/ws/serial');"
"ws.onmessage=function(e){"
"var t=document.getElementById('serialTerm');t.textContent+=e.data;"
"if(t.textContent.length>10000)t.textContent=t.textContent.slice(-8000);"
"if(autoScroll)t.scrollTop=t.scrollHeight;};}"
"function sendSerial(m){if(ws)ws.send(m+'\\r\\n');}"
"function toggleAuto(){autoScroll=document.getElementById('serialAuto').checked;}"
"function exitRecovery(){fetch('/api/exit',{method:'POST'})}"
"loadFiles();"
"</script></body></html>"
    ));
}
