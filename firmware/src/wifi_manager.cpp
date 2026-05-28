#include "wifi_manager.h"
#include "config.h"
#include "storage.h"
#include "led_control.h"
#include "log.h"
#include <WiFi.h>

static bool parseBSSID(const String &text, uint8_t out[6]) {
    if (text.length() != 17) return false;
    int values[6];
    if (sscanf(text.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; i++) {
        if (values[i] < 0 || values[i] > 255) return false;
        out[i] = (uint8_t)values[i];
    }
    return true;
}

bool connectWiFi() {
    if (cfgSSID.length() == 0) return false;

    for (int retry = 1; retry <= 3; retry++) {
        unsigned long startMs = millis();
        LOGI("WiFi connecting, attempt=%d/3 ssid=%s", retry, cfgSSID.c_str());
        WiFi.mode(WIFI_STA);
        uint8_t bssid[6];
        if (cfgWiFiChannel > 0 && parseBSSID(cfgWiFiBSSID, bssid)) {
            // Directed reconnect skips a full channel scan when the router/AP has
            // not changed. If it is stale, this attempt times out and retry falls
            // through the same failure/backoff handling as a normal connection.
            WiFi.begin(cfgSSID.c_str(), cfgPass.c_str(), cfgWiFiChannel, bssid);
        } else {
            WiFi.begin(cfgSSID.c_str(), cfgPass.c_str());
        }

        unsigned long t0 = millis();
        bool ok = true;
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - t0 > (unsigned long)WIFI_TIMEOUT) {
                LOGW("WiFi failed: timeout");
                ok = false;
                break;
            }
            ledToggleIdle();
            delay(300);
        }
        ledOffIfIdle();
        if (ok) {
            LOGI("WiFi OK, ip=%s rssi=%d elapsed=%lums", WiFi.localIP().toString().c_str(), WiFi.RSSI(), millis() - startMs);
            // Refresh AP metadata after every successful connection because mesh
            // or multi-AP networks may steer the device to a different BSSID.
            saveWiFiApInfo(WiFi.BSSIDstr(), WiFi.channel());
            return true;
        }
        // Disconnect and delay before next attempt to give router some breathing room
        WiFi.disconnect(true);
        delay(1500);
    }
    return false;
}

void stopWiFi() {
    if (WiFi.getMode() == WIFI_OFF) return;
    unsigned long startMs = millis();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    LOGI("WiFi off, elapsed=%lums", millis() - startMs);
}

WiFiSessionGuard::~WiFiSessionGuard() {
    stopNow();
}

void WiFiSessionGuard::stopNow() {
    if (!active) return;
    stopWiFi();
    active = false;
}

void WiFiSessionGuard::release() {
    active = false;
}
