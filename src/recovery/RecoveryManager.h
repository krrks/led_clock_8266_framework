#pragma once
// RecoveryManager — self-contained ESP8266 boot/recovery module
//
// Drop-in reusable module. Copy src/recovery/ + src/web/SerialMonitor.*
// to any ESP8266 project. Depends on: ESPAsyncTCP, ESPAsyncWebServer,
// ArduinoJson, LittleFS.
//
// Triggers:
//   1. Button hold (triggerPin) during boot window
//   2. ESP reset reason: Exception or Watchdog
//   3. RTC memory flag (software trigger via trigger())
//
// Recovery mode:
//   - Tries STA with provided SSID/password
//   - Falls back to AP mode on failure
//   - Serves recovery web UI on webPort (default 80)
//   - Provides: firmware OTA, file management, wireless serial monitor

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>

// ─── Serial redirect helper ─────────────────────────────────────────────
class SerialRedirect : public Print {
public:
    SerialRedirect(HardwareSerial& hw, bool wired, bool wireless);
    void   setWiredEnabled(bool en);
    void   setWirelessEnabled(bool en);
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buf, size_t len) override;
    size_t printf(const char* fmt, ...) __attribute__((format(printf,2,3)));
    int    available();
    int    read();
    int    peek();
    void   flush();
private:
    HardwareSerial& _hw;
    bool _wired;
    bool _wireless;
    static const int BUF_SZ = 8192;
    volatile int _head;
    volatile int _tail;
    uint8_t _buf[BUF_SZ];
};

extern SerialRedirect SerialOut;

// ─── Recovery Manager ───────────────────────────────────────────────────
class RecoveryManager {
public:
    // Configuration (set before begin())
    uint8_t  triggerPin    = 0;      // GPIO for button (0 = none)
    uint16_t bootWindowMs  = 3000;   // Boot button watch window
    String   apSSID        = "RECOVERY";  // AP mode SSID
    String   apPassword    = "";     // AP password (empty = open)
    uint16_t webPort       = 80;     // Recovery web server port
    String   staSSID;                // STA SSID (from config)
    String   staPassword;            // STA password (from config)
    bool     serialMonitorEnabled  = true;   // wired serial output
    bool     wirelessSerialEnabled = false;  // web serial via WebSocket

    void begin();           // Call in setup() — checks triggers, starts recovery if needed
    void loop();            // Call in loop() — handles DNS + serial WS broadcast
    void trigger();         // Software trigger — write RTC flag + reboot
    bool isActive() const { return _active; }

    static RecoveryManager& get();

private:
    bool _active = false;
    bool _apMode = false;

    AsyncWebServer* _server = nullptr;
    DNSServer       _dnsServer;
    AsyncWebSocket* _wsSerial = nullptr;

    static const uint32_t RTC_MAGIC = 0xC10CFA11UL;
    struct RTCData { uint32_t magic; uint32_t enterRecovery; };

    void _startAP();
    void _startSTA();
    void _startWebServer();
    void _handleUpload(AsyncWebServerRequest *request,
                       String filename, size_t index,
                       uint8_t *data, size_t len, bool final);
    void _broadcastSerial();
    void _saveWiFiConfig();
    String _getRecoveryHTML();

    // Serial command handlers (one per command; register in _commands[] table)
    typedef void (RecoveryManager::*CmdFn)(AsyncWebSocketClient*);
    struct Cmd { const char* name; CmdFn fn; };
    void _wsHelp(AsyncWebSocketClient* c);
    void _wsStatus(AsyncWebSocketClient* c);
    void _wsHeap(AsyncWebSocketClient* c);
    void _wsReboot(AsyncWebSocketClient* c);
    void _wsWifi(AsyncWebSocketClient* c);
    void _wsClear(AsyncWebSocketClient* c);
    void _wsLs(AsyncWebSocketClient* c);
    void _wsInfo(AsyncWebSocketClient* c);
    void _wsScan(AsyncWebSocketClient* c);
    void _wsDf(AsyncWebSocketClient* c);
    void _wsReset(AsyncWebSocketClient* c);
};
