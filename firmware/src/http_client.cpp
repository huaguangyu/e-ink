#include "http_client.h"
#include "config.h"
#include "led_control.h"
#include "log.h"
#include "storage.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <sys/time.h>

static HTTPClient http;
bool lastHeartbeatPersist = true;  // default: write to flash (backward compatible)

// ── Heartbeat ──────────────────────────────────────────────

int sendHeartbeat(float voltage, int pct, const char *ip,
                  const char *wake, int uptime_s, int free_heap,
                  int rssi, const char *ssid, const char *firmware) {
    if (!hasConsole()) return -1;
    String url = cfgConsoleUrl + "/api/heartbeat";
    // Any real HTTP transaction should make the LED steady-on. The guard also
    // handles early returns on parse/network errors.
    HttpLedGuard httpLed;
    http.setTimeout(CONSOLE_TIMEOUT);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Build device timestamp from system time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    char deviceTs[24];
    snprintf(deviceTs, sizeof(deviceTs), "%04d-%02d-%02d %02d:%02d",
             1900 + tm_info->tm_year, 1 + tm_info->tm_mon, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min);

    // Build JSON body
    String mac = WiFi.macAddress();
    mac.replace(":", "");

    JsonDocument doc;
    doc["mac"]        = mac;
    doc["voltage"]    = voltage;
    doc["pct"]        = pct;
    doc["ip"]         = ip;
    doc["wake"]       = wake;
    doc["uptime_s"]   = uptime_s;
    doc["device_ts"]  = deviceTs;
    doc["free_heap"]  = free_heap;
    doc["rssi"]       = rssi;
    doc["ssid"]       = ssid;
    doc["firmware"]   = firmware;
    doc["sleep_min"]  = cfgSleepMin;

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    int sleepMin = -1;

    if (code == 200) {
        String resp = http.getString();
        JsonDocument rdoc;
        DeserializationError err = deserializeJson(rdoc, resp);
        if (err) {
            LOGE("Heartbeat parse error: %s", err.c_str());
            http.end();
            return -1;
        }
        sleepMin = rdoc["sleep_min"] | -1;
        lastHeartbeatPersist = rdoc["persist"] | true;

        LOGI("Heartbeat OK, sleep_min=%d persist=%d", sleepMin, lastHeartbeatPersist);

        // If console says restart, do it
        if (rdoc["restart"] | false) {
            LOGW("Console requested restart");
            http.end();
            delay(100);
            ESP.restart();
        }
    } else {
        LOGW("Heartbeat failed: HTTP %d", code);
    }

    http.end();
    return sleepMin;
}

// ── Fetch Tasks ─────────────────────────────────────────────

int fetchPendingTasks(const char *mac, ConsoleTask *out, int maxTasks) {
    if (!hasConsole()) return 0;
    String url = cfgConsoleUrl + "/api/tasks?mac=" + String(mac);
    // Keep task polling visible as network activity even when the task list is empty.
    HttpLedGuard httpLed;
    http.setTimeout(CONSOLE_TIMEOUT);
    http.begin(url);

    int code = http.GET();
    if (code != 200) {
        LOGW("Fetch tasks failed: HTTP %d", code);
        http.end();
        return 0;
    }

    String resp = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        LOGE("Fetch tasks parse error: %s", err.c_str());
        return 0;
    }

    JsonArray tasks = doc["tasks"].as<JsonArray>();
    int count = 0;
    for (JsonObject t : tasks) {
        if (count >= maxTasks) break;
        out[count].id = t["id"];
        strncpy(out[count].type, t["type"] | "", sizeof(out[count].type) - 1);
        out[count].type[sizeof(out[count].type) - 1] = '\0';

        String paramsStr;
        serializeJson(t["params"], paramsStr);
        strncpy(out[count].params, paramsStr.c_str(), sizeof(out[count].params) - 1);
        out[count].params[sizeof(out[count].params) - 1] = '\0';

        count++;
    }

    LOGI("Tasks OK, count=%d", count);
    return count;
}

// ── Report Task Result ──────────────────────────────────────

void reportTaskResult(int taskId, bool success, const char *data) {
    if (!hasConsole()) return;
    String url = cfgConsoleUrl + "/api/tasks/" + String(taskId) + "/result";
    // Result reporting is part of the same HTTP activity model as heartbeat/tasks.
    HttpLedGuard httpLed;
    http.setTimeout(CONSOLE_TIMEOUT);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["success"] = success;
    if (data && strlen(data) > 0) {
        doc["data"] = serialized(data);
    }

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    if (code == 200) {
        LOGI("Task %d result reported: %s", taskId, success ? "OK" : "FAIL");
    } else {
        LOGW("Task %d result report failed: HTTP %d", taskId, code);
    }
    http.end();
}

// ── Sync Time ───────────────────────────────────────────────

bool syncTimeFromServer() {
    HttpLedGuard httpLed;
    LOGI("Sync time via NTP");
    configTime(NTP_UTC_OFFSET, 0, "ntp.aliyun.com", "pool.ntp.org");

    // Wait up to 5 seconds for NTP to complete
    for (int i = 0; i < 50; i++) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        if (tv.tv_sec > 1700000000) {
            struct tm *tm_info = localtime(&tv.tv_sec);
            LOGI("NTP time synced: %04d-%02d-%02d %02d:%02d:%02d",
                 1900 + tm_info->tm_year, 1 + tm_info->tm_mon, tm_info->tm_mday,
                 tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
            return true;
        }
        delay(100);
    }
    LOGW("NTP sync timed out");
    return false;
}
