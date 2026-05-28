#ifndef EINK_WIFI_MANAGER_H
#define EINK_WIFI_MANAGER_H

#include <Arduino.h>

// Connect to stored WiFi credentials.
// When previous BSSID/channel metadata exists, the implementation tries a directed
// connection first to reduce scan time and radio-on duration. Returns true only
// after WL_CONNECTED and AP metadata has been refreshed in NVS.
bool connectWiFi();

// Fully shut down the WiFi radio after network work is complete.
// Call before display/interaction/sleep paths so manual page switching uses cached
// data and does not keep the high-current radio active.
void stopWiFi();

class WiFiSessionGuard {
public:
    ~WiFiSessionGuard();
    // Stop immediately and mark the guard inactive. Used before display or sleep.
    void stopNow();
    // Transfer WiFi ownership to another subsystem, currently the captive portal.
    void release();

private:
    bool active = true;
};

#endif // EINK_WIFI_MANAGER_H
