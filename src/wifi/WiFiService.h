#pragma once
// WiFiService — WiFi STA+AP management
// Replaces maakbaas/esp8266-iot-framework WiFiManager.

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>

class WiFiService {
public:
    // Try STA with saved credentials; on fail, start AP with given name.
    // timeoutMs: how long to wait for STA connection (0 = no wait, return immediately)
    void begin(const char* apName, unsigned long timeoutMs = 15000);

    // Must be called in loop(). Handles DNS redirect for captive portal.
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
    bool    _apMode = false;
    bool    _started = false;
    String  _ssid;
    String  _pass;
    DNSServer _dnsServer;
};

extern WiFiService WiFiManager;   // alias for migration compatibility
