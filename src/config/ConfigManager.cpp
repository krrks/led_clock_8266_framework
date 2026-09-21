// ConfigManager.cpp — LittleFS JSON config storage
#include "ConfigManager.h"
#include "LittleFS.h"
#include <ArduinoJson.h>

// Local-only secrets (gitignored); public clones fall back to empty key.
#if __has_include("Secrets.h")
#include "Secrets.h"
#endif
#ifndef WEATHER_API_KEY
#define WEATHER_API_KEY ""
#endif

static const char* CONFIG_PATH = "/config.json";

const configData configDefaults PROGMEM = {
    "LED Clock",
    "en",
    0,   // brightness (dim)
    1,   // brightDim
    16,  // brightMed
    255, // brightBrt
    true, // use24h
    0,   // rotation
    2,   // flip (hardcoded V-flip in ClockDisplay.cpp, kept for reference)
    80,  // scrollSpeed
    0,   // colorIndex (COLOR_PRESETS[0] = white)
    "HKT-8",
    true, // wifiEnabled
    "",   // wifiSSID
    "",   // wifiPassword
    true, // defaultWeather
    WEATHER_API_KEY,  // weatherApiKey (from local Secrets.h; empty in public builds)
    "Guangzhou,CN",
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

    // IMPORTANT: defaults come from `data` (RAM copy of configDefaults made in
    // begin()), never from configDefaults (PROGMEM) directly. ArduinoJson
    // dereferences the default value when a key is missing, and byte loads
    // from flash crash with LoadStoreError on ESP8266.
    #define LOAD_STR(key, field) do { \
        const char* _v = doc[key] | (const char*)nullptr; \
        if (_v) strlcpy(field, _v, sizeof(field)); \
    } while (0)

    LOAD_STR("projectName",   data.projectName);
    LOAD_STR("language",      data.language);
    data.brightness     = doc["brightness"]     | data.brightness;
    data.brightDim      = doc["brightDim"]      | data.brightDim;
    data.brightMed      = doc["brightMed"]      | data.brightMed;
    data.brightBrt      = doc["brightBrt"]      | data.brightBrt;
    data.use24h         = doc["use24h"]         | data.use24h;
    data.rotation       = doc["rotation"]       | data.rotation;
    data.flip           = doc["flip"]           | data.flip;
    data.scrollSpeed    = doc["scrollSpeed"]    | data.scrollSpeed;
    data.colorIndex     = doc["colorIndex"]     | data.colorIndex;
    LOAD_STR("timezone",       data.timezone);
    data.wifiEnabled    = doc["wifiEnabled"]    | data.wifiEnabled;
    LOAD_STR("wifiSSID",       data.wifiSSID);
    LOAD_STR("wifiPassword",   data.wifiPassword);
    data.defaultWeather = doc["defaultWeather"] | data.defaultWeather;
    LOAD_STR("weatherApiKey",  data.weatherApiKey);
    LOAD_STR("weatherCity",    data.weatherCity);
    data.manualHour     = doc["manualHour"]     | data.manualHour;
    data.manualMinute   = doc["manualMinute"]   | data.manualMinute;
    data.manualDay      = doc["manualDay"]      | data.manualDay;
    data.manualMonth    = doc["manualMonth"]    | data.manualMonth;
    data.manualYear     = doc["manualYear"]     | data.manualYear;
    data.manualWeekday  = doc["manualWeekday"]  | data.manualWeekday;
    data.serialMonitorEnabled  = doc["serialMonitor"]  | data.serialMonitorEnabled;
    data.wirelessSerialEnabled = doc["wirelessSerial"] | data.wirelessSerialEnabled;

    #undef LOAD_STR

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
    doc["colorIndex"]      = data.colorIndex;
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
