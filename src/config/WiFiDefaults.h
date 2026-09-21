#pragma once
// WiFiDefaults.h — firmware fallback WiFi credentials
// Used when no WiFi is saved in config.json (e.g. after uploadfs or factory reset).
// Real credentials live in the local, gitignored Secrets.h (see Secrets.h.example).

#if __has_include("Secrets.h")
#include "Secrets.h"
#endif

#ifndef WIFI_DEFAULT_SSID
#define WIFI_DEFAULT_SSID     "YOUR_SSID"
#endif

#ifndef WIFI_DEFAULT_PASSWORD
#define WIFI_DEFAULT_PASSWORD ""
#endif
