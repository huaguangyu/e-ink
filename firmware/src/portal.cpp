#include "portal.h"
#include "config.h"
#include "storage.h"
#include "battery.h"
#include "log.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

#include "../data/portal_html.h"

bool portalActive  = false;
bool wifiConnected = false;
static float portalBatteryV = 0;

static bool     wifiConnecting = false;
static String   lastWifiError  = "";
static bool     pendingRestart = false;
static unsigned long restartAtMillis = 0;

static WebServer webServer(80);
static DNSServer dnsServer;

static const int PORTAL_MAX_SSID = 32;
static const int PORTAL_MAX_PASS = 64;

static String jsonEscape(const String &input) {
    String out;
    out.reserve(input.length() + 8);
    for (unsigned int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

static String sanitizeSSID(const String &input) {
    String result = input.substring(0, PORTAL_MAX_SSID);
    result.trim();
    result.replace("<", "");
    result.replace(">", "");
    result.replace("\"", "");
    String cleaned;
    for (unsigned int i = 0; i < result.length(); i++) {
        char c = result.charAt(i);
        // Keep printable ASCII and UTF-8 bytes so Chinese SSIDs survive, while
        // dropping control characters that can break the JSON/HTML response.
        if (c >= 32 || (c & 0x80)) cleaned += c;
    }
    return cleaned;
}

static String sanitizePass(const String &input) {
    String result = input.substring(0, PORTAL_MAX_PASS);
    result.trim();
    result.replace("<", "");
    result.replace(">", "");
    return result;
}

void startCaptivePortal() {
    portalBatteryV = readBatteryVoltage();
    String mac = WiFi.macAddress();
    String apName = String(AP_NAME_PREFIX) + "-" + mac.substring(mac.length() - 5);
    apName.replace(":", "");

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apName.c_str());
    delay(100);

    LOGI("AP started: %s ip=%s", apName.c_str(), WiFi.softAPIP().toString().c_str());

    dnsServer.start(53, "*", WiFi.softAPIP());

    webServer.on("/", HTTP_GET, []() {
        webServer.send(200, "text/html", PORTAL_HTML);
    });

    webServer.on("/scan", HTTP_GET, []() {
        int n = WiFi.scanNetworks();
        struct NetInfo { String ssid; int rssi; bool secure; };
        NetInfo best[32];
        int count = 0;
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) continue;
            int rssi = WiFi.RSSI(i);
            bool secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            int found = -1;
            for (int j = 0; j < count; j++)
                if (best[j].ssid == ssid) { found = j; break; }
            if (found >= 0) {
                // Collapse duplicate SSIDs from mesh/multi-AP networks and show
                // only the strongest candidate in the captive portal list.
                if (rssi > best[found].rssi) { best[found].rssi = rssi; best[found].secure = secure; }
            } else if (count < 32) {
                best[count++] = { ssid, rssi, secure };
            }
        }
        for (int i = 0; i < count - 1; i++)
            for (int j = i + 1; j < count; j++)
                if (best[j].rssi > best[i].rssi) { NetInfo tmp = best[i]; best[i] = best[j]; best[j] = tmp; }

        String json = "{\"networks\":[";
        for (int i = 0; i < count; i++) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + best[i].ssid + "\",\"rssi\":" + String(best[i].rssi) +
                    ",\"secure\":" + String(best[i].secure ? "true" : "false") + "}";
        }
        json += "]}";
        webServer.sendHeader("Access-Control-Allow-Origin", "*");
        webServer.send(200, "application/json", json);
    });

    webServer.on("/info", HTTP_GET, []() {
        String json = "{\"mac\":\"" + WiFi.macAddress() +
                      "\",\"battery\":\"" + String(portalBatteryV, 2) +
                      "V\",\"ssid\":\"" + jsonEscape(cfgSSID) +
                      "\",\"pass\":\"" + jsonEscape(cfgPass) +
                      "\",\"console_url\":\"" + jsonEscape(cfgConsoleUrl) +
                      "\",\"has_console\":" + (hasConsole() ? "true" : "false") +
                      ",\"quota_url\":\"" + jsonEscape(cfgQuotaUrl) +
                      "\",\"sleep_min\":" + String(cfgSleepMin) + "}";
        webServer.send(200, "application/json", json);
    });

    webServer.on("/status", HTTP_GET, []() {
        String json = "{\"state\":\"";
        if (WiFi.status() == WL_CONNECTED) json += "connected\",\"ip\":\"" + WiFi.localIP().toString() + "\"";
        else if (wifiConnecting) json += "connecting\"";
        else if (lastWifiError.length() > 0) json += "failed\",\"error\":\"" + lastWifiError + "\"";
        else json += "idle\"";
        json += "}";
        webServer.sendHeader("Access-Control-Allow-Origin", "*");
        webServer.send(200, "application/json", json);
    });

    webServer.on("/save_wifi", HTTP_POST, []() {
        String ssid = sanitizeSSID(webServer.arg("ssid"));
        String pass = sanitizePass(webServer.arg("pass"));
        String quotaUrl = webServer.arg("quota_url");
        quotaUrl.trim();

        if (ssid.length() == 0) {
            webServer.send(200, "application/json", "{\"ok\":false,\"msg\":\"SSID empty\"}");
            return;
        }

        // Password can be empty (open WiFi). If empty and no old password, that's OK.
        if (pass.length() > 0 && pass.length() < 8) {
            webServer.send(200, "application/json", "{\"ok\":false,\"msg\":\"Password must be at least 8 chars\"}");
            return;
        }

        if (quotaUrl.length() == 0) {
            webServer.send(200, "application/json", "{\"ok\":false,\"msg\":\"Quota URL required\"}");
            return;
        }

        LOGI("Portal connecting to ssid=%s", ssid.c_str());
        wifiConnecting = true;
        lastWifiError = "";

        WiFi.mode(WIFI_AP_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < (unsigned long)WIFI_TIMEOUT) delay(300);

        wifiConnecting = false;

        if (WiFi.status() == WL_CONNECTED) {
            saveWiFiConfig(ssid, pass);

            String consoleUrl = webServer.arg("console_url");
            saveConsoleUrl(consoleUrl);
            LOGI("Console URL saved: %s", cfgConsoleUrl.c_str());

            saveQuotaUrl(quotaUrl);
            LOGI("Quota URL saved: %s", cfgQuotaUrl.c_str());

            String sleepMinStr = webServer.arg("sleep_min");
            int sleepMin = sleepMinStr.toInt();
            if (sleepMin > 0) {
                saveSleepMin(sleepMin);
                LOGI("Sleep min saved: %d", cfgSleepMin);
            }

            wifiConnected = true;
            lastWifiError = "";
            LOGI("Portal WiFi OK, ip=%s", WiFi.localIP().toString().c_str());
            webServer.send(200, "application/json", "{\"ok\":true}");
            // Delay restart so the browser has time to receive the success JSON.
            pendingRestart = true;
            restartAtMillis = millis() + 10000;
        } else {
            lastWifiError = "TIMEOUT";
            WiFi.disconnect();
            WiFi.mode(WIFI_AP_STA);
            webServer.send(200, "application/json", "{\"ok\":false,\"msg\":\"Connection failed\"}");
        }
    });

    webServer.on("/restart", HTTP_POST, []() {
        webServer.send(200, "application/json", "{\"ok\":true}");
        delay(1000);
        ESP.restart();
    });

    webServer.onNotFound([]() {
        String path = webServer.uri();
        if (path == "/generate_204" || path == "/gen_204" || path == "/hotspot-detect.html") {
            webServer.send(204); return;
        }
        if (path.endsWith(".ico") || path.endsWith(".png")) { webServer.send(404); return; }
        webServer.sendHeader("Location", "http://" + WiFi.softAPIP().toString());
        webServer.send(302, "text/plain", "");
    });

    webServer.begin();
    portalActive = true;
    LOGI("Captive portal started");
}

void handlePortalClients() {
    dnsServer.processNextRequest();
    webServer.handleClient();
    if (pendingRestart && millis() >= restartAtMillis) {
        LOGI("Portal: deferred restart");
        delay(200);
        ESP.restart();
    }
}
