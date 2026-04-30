// ConfigManager.cpp — LittleFS JSON config storage
#include "ConfigManager.h"
#include "LittleFS.h"
#include <ArduinoJson.h>

static const char* CONFIG_PATH = "/config.json";

const configData configDefaults PROGMEM = {
    "LED Clock",
    "en",
    0,   // brightness (dim)
    1,   // brightDim
    128, // brightMed
    255, // brightBrt
    true, // use24h
    0,   // rotation
    0,   // flip
    80,  // scrollSpeed
    "HKT-8",
    true, // wifiEnabled
    "",   // wifiSSID
    "",   // wifiPassword
    true, // defaultWeather
    "",   // weatherApiKey
    "Hong Kong",
    12, 0, 1, 1, 2025, 1,
    true,  // serialMonitorEnabled
    false  // wirelessSerialEnabled
};

ConfigManager configManager;

void ConfigManager::begin() {
    memcpy_P(&data, &configDefaults, sizeof(configData));
    if (!loadFromFS()) {
        saveToFS();  // write defaults on first boot
        Serial.println(F("[Config] defaults written"));
    }
    Serial.printf("[Config] loaded: bright=%d rot=%d flip=%d spd=%d wifi=%d wx=%d\n",
                  data.brightness, data.rotation, data.flip,
                  data.scrollSpeed, data.wifiEnabled, data.defaultWeather);
}

void ConfigManager::save() {
    saveToFS();
    if (_onSave) _onSave();
}

bool ConfigManager::loadFromFS() {
    File f = LittleFS.open(CONFIG_PATH, "r");
    if (!f) {
        Serial.println(F("[Config] no saved config"));
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Config] JSON parse error: %s\n", err.c_str());
        return false;
    }

    strlcpy(data.projectName,      doc["projectName"]      | configDefaults.projectName,    sizeof(data.projectName));
    strlcpy(data.language,         doc["language"]         | configDefaults.language,       sizeof(data.language));
    data.brightness      = doc["brightness"]      | configDefaults.brightness;
    data.brightDim       = doc["brightDim"]       | configDefaults.brightDim;
    data.brightMed       = doc["brightMed"]       | configDefaults.brightMed;
    data.brightBrt       = doc["brightBrt"]       | configDefaults.brightBrt;
    data.use24h          = doc["use24h"]          | configDefaults.use24h;
    data.rotation        = doc["rotation"]        | configDefaults.rotation;
    data.flip            = doc["flip"]            | configDefaults.flip;
    data.scrollSpeed     = doc["scrollSpeed"]     | configDefaults.scrollSpeed;
    strlcpy(data.timezone,        doc["timezone"]        | configDefaults.timezone,        sizeof(data.timezone));
    data.wifiEnabled     = doc["wifiEnabled"]     | configDefaults.wifiEnabled;
    strlcpy(data.wifiSSID,        doc["wifiSSID"]        | configDefaults.wifiSSID,        sizeof(data.wifiSSID));
    strlcpy(data.wifiPassword,    doc["wifiPassword"]    | configDefaults.wifiPassword,    sizeof(data.wifiPassword));
    data.defaultWeather  = doc["defaultWeather"]  | configDefaults.defaultWeather;
    strlcpy(data.weatherApiKey,   doc["weatherApiKey"]   | configDefaults.weatherApiKey,   sizeof(data.weatherApiKey));
    strlcpy(data.weatherCity,     doc["weatherCity"]     | configDefaults.weatherCity,     sizeof(data.weatherCity));
    data.manualHour      = doc["manualHour"]      | configDefaults.manualHour;
    data.manualMinute    = doc["manualMinute"]    | configDefaults.manualMinute;
    data.manualDay       = doc["manualDay"]       | configDefaults.manualDay;
    data.manualMonth     = doc["manualMonth"]     | configDefaults.manualMonth;
    data.manualYear      = doc["manualYear"]      | configDefaults.manualYear;
    data.manualWeekday   = doc["manualWeekday"]   | configDefaults.manualWeekday;
    data.serialMonitorEnabled  = doc["serialMonitor"]  | configDefaults.serialMonitorEnabled;
    data.wirelessSerialEnabled = doc["wirelessSerial"] | configDefaults.wirelessSerialEnabled;

    return true;
}

bool ConfigManager::saveToFS() {
    JsonDocument doc;

    doc["projectName"]     = data.projectName;
    doc["language"]        = data.language;
    doc["brightness"]      = data.brightness;
    doc["brightDim"]       = data.brightDim;
    doc["brightMed"]       = data.brightMed;
    doc["brightBrt"]       = data.brightBrt;
    doc["use24h"]          = data.use24h;
    doc["rotation"]        = data.rotation;
    doc["flip"]            = data.flip;
    doc["scrollSpeed"]     = data.scrollSpeed;
    doc["timezone"]        = data.timezone;
    doc["wifiEnabled"]     = data.wifiEnabled;
    doc["wifiSSID"]        = data.wifiSSID;
    doc["wifiPassword"]    = data.wifiPassword;
    doc["defaultWeather"]  = data.defaultWeather;
    doc["weatherApiKey"]   = data.weatherApiKey;
    doc["weatherCity"]     = data.weatherCity;
    doc["manualHour"]      = data.manualHour;
    doc["manualMinute"]    = data.manualMinute;
    doc["manualDay"]       = data.manualDay;
    doc["manualMonth"]     = data.manualMonth;
    doc["manualYear"]      = data.manualYear;
    doc["manualWeekday"]   = data.manualWeekday;
    doc["serialMonitor"]   = data.serialMonitorEnabled;
    doc["wirelessSerial"]  = data.wirelessSerialEnabled;

    File f = LittleFS.open(CONFIG_PATH, "w");
    if (!f) {
        Serial.println(F("[Config] ERROR: cannot open for write"));
        return false;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}

void ConfigManager::resetToDefaults() {
    memcpy_P(&data, &configDefaults, sizeof(configData));
    saveToFS();
    Serial.println(F("[Config] reset to defaults"));
}
