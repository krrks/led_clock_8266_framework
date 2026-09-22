#pragma once
// WiFiService — WiFi STA+AP management
// Replaces maakbaas/esp8266-iot-framework WiFiManager.

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>

class WiFiService {
public:
    // Non-blocking: starts STA with saved credentials and returns immediately.
    // loop() completes the connection wait and switches to AP on timeout.
    // timeoutMs: how long loop() waits for STA connection before AP fallback.
    void begin(const char* apName, unsigned long timeoutMs = 15000);

    // Must be called in loop() (or any periodic context): completes the STA
    // connection wait, switches to AP on timeout, handles captive-portal DNS.
    void loop();

    // True if device is in AP mode (not connected to any STA)
    bool isCaptivePortal() const { return _apMode; }

    // True if WiFi is connected (STA mode with IP)
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }

    String localIP() const { return WiFi.localIP().toString(); }

    // Credentials (may be set via web config)
    void setCredentials(const char* ssid, const char* password);
    const char* ssid() const     { return _ssid.c_str(); }
    const char* password() const { return _pass.c_str(); }

private:
    void _enterAP();

    bool    _apMode = false;
    bool    _started = false;
    bool    _announced = false;
    String  _ssid;
    String  _pass;
    String  _apName;
    unsigned long _connectTimeout = 15000;
    unsigned long _tConnectStart  = 0;
    DNSServer _dnsServer;
};

extern WiFiService WiFiManager;   // alias for migration compatibility
