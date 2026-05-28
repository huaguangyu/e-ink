#ifndef EINK_STORAGE_H
#define EINK_STORAGE_H

#include <Arduino.h>

// Runtime config populated from NVS by loadConfig(). These globals mirror the
// active firmware configuration so call sites do not repeatedly open Preferences.
extern String cfgSSID;
extern String cfgPass;
extern String cfgConsoleUrl;
extern String cfgQuotaUrl;
extern String cfgWiFiBSSID;
extern int    cfgWiFiChannel;
extern int    cfgSleepMin;

// RTC-backed UI state survives deep sleep and is intentionally small. It is used
// for page continuity and retry counters, not as durable configuration storage.
extern RTC_DATA_ATTR int rtcRetryCount;
extern RTC_DATA_ATTR int rtcLastPage;

// Load all config from NVS
void loadConfig();

// Save WiFi credentials
void saveWiFiConfig(const String &ssid, const String &pass);

// Save AP metadata for faster reconnects. Credentials and AP metadata are stored
// separately so changing routers only invalidates the optimization, not WiFi login.
void saveWiFiApInfo(const String &bssid, int channel);

// Save console URL (empty string = standalone mode)
void saveConsoleUrl(const String &url);

// Returns true when a console URL is configured (console-controlled mode).
bool hasConsole();

// Save quota API URL (required, never empty in normal operation)
void saveQuotaUrl(const String &url);

// Save sleep interval
void saveSleepMin(int minutes);

// Retry counter (RTC-backed)
int  getRetryCount();
void setRetryCount(int count);
void resetRetryCount();

#endif // EINK_STORAGE_H
