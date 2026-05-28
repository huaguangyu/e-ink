#include "storage.h"
#include "config.h"
#include "log.h"
#include <Preferences.h>

static Preferences prefs;
// Bump this when persisted key semantics change. A mismatch deliberately falls
// back to defaults instead of trying to interpret unknown old data.
static const int CONFIG_VERSION = 3;
static const int MAX_CONSOLE_URL_LEN = 128;
static const int MAX_QUOTA_URL_LEN = 128;

String cfgSSID;
String cfgPass;
String cfgConsoleUrl;
String cfgQuotaUrl;
String cfgWiFiBSSID;
int    cfgWiFiChannel;
int    cfgSleepMin;

// RTC-backed state
RTC_DATA_ATTR int rtcRetryCount = 0;
RTC_DATA_ATTR int rtcLastPage = 0;

static String normalizeConsoleUrl(const String &url) {
    String result = url.substring(0, MAX_CONSOLE_URL_LEN);
    result.trim();
    result.replace(" ", "");
    result.replace("\t", "");
    result.replace("\r", "");
    result.replace("\n", "");

    if (result.length() == 0) return "";
    if (!result.startsWith("http://") && !result.startsWith("https://")) {
        result = "http://" + result;
    }
    while (result.endsWith("/") && result.length() > 8) {
        result.remove(result.length() - 1);
    }
    return result;
}

static String normalizeQuotaUrl(const String &url) {
    String result = url.substring(0, MAX_QUOTA_URL_LEN);
    result.trim();
    result.replace(" ", "");
    result.replace("\t", "");
    result.replace("\r", "");
    result.replace("\n", "");

    if (result.length() == 0) return "";
    if (!result.startsWith("http://") && !result.startsWith("https://")) {
        result = "http://" + result;
    }
    while (result.endsWith("/") && result.length() > 8) {
        result.remove(result.length() - 1);
    }
    return result;
}

void loadConfig() {
    prefs.begin("eink", true);
    int version = prefs.getInt("cfg_version", 0);
    if (version != CONFIG_VERSION) {
        LOGW("Config version mismatch (%d != %d), using defaults", version, CONFIG_VERSION);
        prefs.end();
        cfgSSID = "";
        cfgPass = "";
        cfgConsoleUrl = "";
        cfgQuotaUrl = "";
        cfgWiFiBSSID = "";
        cfgWiFiChannel = 0;
        cfgSleepMin = DEFAULT_SLEEP_MIN;
        return;
    }

    cfgSSID = prefs.getString("ssid", "");
    cfgPass = prefs.getString("pass", "");
    cfgWiFiBSSID = prefs.getString("wifi_bssid", "");
    cfgWiFiChannel = prefs.getInt("wifi_channel", 0);
    String savedConsoleUrl = prefs.getString("console_url", "");
    cfgConsoleUrl = normalizeConsoleUrl(savedConsoleUrl);
    String savedQuotaUrl = prefs.getString("quota_url", "");
    cfgQuotaUrl = normalizeQuotaUrl(savedQuotaUrl);
    cfgSleepMin = prefs.getInt("sleep_min", DEFAULT_SLEEP_MIN);
    prefs.end();

    if (cfgSleepMin < 1 || cfgSleepMin > 1440) cfgSleepMin = DEFAULT_SLEEP_MIN;
}

void saveWiFiConfig(const String &ssid, const String &pass) {
    prefs.begin("eink", false);
    prefs.putInt("cfg_version", CONFIG_VERSION);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.putString("wifi_bssid", "");
    prefs.putInt("wifi_channel", 0);
    prefs.end();
    cfgSSID = ssid;
    cfgPass = pass;
    cfgWiFiBSSID = "";
    cfgWiFiChannel = 0;
}

void saveWiFiApInfo(const String &bssid, int channel) {
    // Fast reconnect metadata is an optimization only. Ignore empty/invalid input
    // and avoid flash writes when the current AP has not changed.
    if (channel <= 0 || bssid.length() == 0) return;
    if (cfgWiFiBSSID == bssid && cfgWiFiChannel == channel) return;
    prefs.begin("eink", false);
    prefs.putInt("cfg_version", CONFIG_VERSION);
    prefs.putString("wifi_bssid", bssid);
    prefs.putInt("wifi_channel", channel);
    prefs.end();
    cfgWiFiBSSID = bssid;
    cfgWiFiChannel = channel;
}

void saveConsoleUrl(const String &url) {
    String normalized = normalizeConsoleUrl(url);
    if (cfgConsoleUrl == normalized) return;
    prefs.begin("eink", false);
    prefs.putInt("cfg_version", CONFIG_VERSION);
    if (normalized.length() == 0) {
        prefs.remove("console_url");
    } else {
        prefs.putString("console_url", normalized);
    }
    prefs.end();
    cfgConsoleUrl = normalized;
}

void saveQuotaUrl(const String &url) {
    String normalized = normalizeQuotaUrl(url);
    if (cfgQuotaUrl == normalized) return;
    prefs.begin("eink", false);
    prefs.putInt("cfg_version", CONFIG_VERSION);
    prefs.putString("quota_url", normalized);
    prefs.end();
    cfgQuotaUrl = normalized;
}

void saveSleepMin(int minutes) {
    if (minutes < 1) minutes = 1;
    if (minutes > 1440) minutes = 1440;
    if (cfgSleepMin == minutes) return;
    prefs.begin("eink", false);
    prefs.putInt("cfg_version", CONFIG_VERSION);
    prefs.putInt("sleep_min", minutes);
    prefs.end();
    cfgSleepMin = minutes;
}

int getRetryCount() {
    return rtcRetryCount;
}

void setRetryCount(int count) {
    rtcRetryCount = count;
}

void resetRetryCount() {
    rtcRetryCount = 0;
}

bool hasConsole() {
    return cfgConsoleUrl.length() > 0;
}
