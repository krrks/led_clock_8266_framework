#pragma once
// NtpClient — lightweight NTP wrapper around ESP8266 core configTime()
// Replaces maakbaas/esp8266-iot-framework timeSync.

#include <Arduino.h>

class NtpClient {
public:
    // Begin NTP sync with POSIX timezone string (e.g. "HKT-8", "PST8PDT,M3.2.0,M11.1.0")
    void begin(const char* timezone);

    // Block until sync completes or timeout (ms). Returns 0 on success.
    int waitForSyncResult(unsigned long timeoutMs);

    // True if time has been synced at least once
    bool isSynced() const { return _synced; }

private:
    bool _synced = false;
};

extern NtpClient timeSync;  // alias for framework compatibility
