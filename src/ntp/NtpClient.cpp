// NtpClient.cpp — NTP time sync
#include "NtpClient.h"
#include <time.h>
#include <coredecls.h>     // settimeofday_cb
#include <sntp.h>          // sntp_*

NtpClient timeSync;  // global alias for framework compatibility

static bool s_synced = false;

static void ntpCallback() {
    s_synced = true;
}

void NtpClient::begin(const char* timezone) {
    settimeofday_cb(ntpCallback);
    if (timezone && strlen(timezone) > 0) {
        // Use the string overload: it sets TZ *before* starting SNTP.
        // The int overload configTime(0,0,...) would clobber newlib's
        // _timezone to UTC on ESP8266 core 3.x (observed: localtime() = UTC).
        configTime(timezone, "pool.ntp.org", "time.nist.gov", "ntp.ubuntu.com");
        Serial.printf("[NTP] tz=%s\n", timezone);
    } else {
        configTime(0, 0, "pool.ntp.org", "time.nist.gov", "ntp.ubuntu.com");
    }
}

int NtpClient::waitForSyncResult(unsigned long timeoutMs) {
    if (_synced || s_synced) {
        _synced = true;
        return 0;
    }

    unsigned long start = millis();
    while (!s_synced && millis() - start < timeoutMs) {
        delay(50);
    }

    if (s_synced) {
        _synced = true;
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        Serial.printf("[NTP] synced: %s\n", buf);
        return 0;
    }
    return -1;
}
