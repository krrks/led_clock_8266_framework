// NtpClient.cpp — NTP time sync
#include "NtpClient.h"
#include <time.h>
#include <coredecls.h>     // settimeofday_cb
#include <sntp.h>          // sntp_*

NtpClient timeSync;  // global alias for framework compatibility

static bool s_synced = false;

static void ntpCallback() {
    s_synced = true;
    timeSync.begin(nullptr);  // re-apply after sync (no-op with null)
}

void NtpClient::begin(const char* timezone) {
    if (timezone && strlen(timezone) > 0) {
        setenv("TZ", timezone, 1);
        tzset();
        Serial.printf("[NTP] tz=%s\n", timezone);
    }
    settimeofday_cb(ntpCallback);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "ntp.ubuntu.com");
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
