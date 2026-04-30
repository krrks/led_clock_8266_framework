#pragma once
// ConfigManager — persistent JSON config on LittleFS
// Replaces maakbaas/esp8266-iot-framework configManager.
// API surface intentionally mirrors the framework for minimal migration churn.

#include <Arduino.h>
#include <functional>

struct configData {
    char    projectName[32];
    char    language[3];
    uint8_t brightness;       // 0=dim, 1=medium, 2=bright
    uint8_t brightDim;        // PWM 1-200
    uint8_t brightMed;        // PWM 1-255
    uint8_t brightBrt;        // PWM 1-255
    bool    use24h;
    uint8_t rotation;         // 0=0°, 1=90°CW, 2=180°, 3=270°CW
    uint8_t flip;             // 0=none, 1=H-flip, 2=V-flip
    uint8_t scrollSpeed;      // ms per column (30-200)
    char    timezone[48];
    bool    wifiEnabled;
    char    wifiSSID[33];
    char    wifiPassword[65];
    bool    defaultWeather;
    char    weatherApiKey[33];
    char    weatherCity[32];
    uint8_t manualHour;
    uint8_t manualMinute;
    uint8_t manualDay;
    uint8_t manualMonth;
    uint16_t manualYear;
    uint8_t manualWeekday;
    bool    serialMonitorEnabled;    // wired serial output
    bool    wirelessSerialEnabled;   // web serial via WebSocket
};

extern const configData configDefaults;

using ConfigSaveCallback = std::function<void()>;

class ConfigManager {
public:
    configData data;

    void begin();
    void save();
    void loop() {}  // no-op, for API compatibility

    void setConfigSaveCallback(ConfigSaveCallback cb) { _onSave = cb; }

    // Load/save JSON from LittleFS
    bool loadFromFS();
    bool saveToFS();

    // Reset to defaults
    void resetToDefaults();

private:
    ConfigSaveCallback _onSave;
};

extern ConfigManager configManager;
