#include "quota.h"
#include "config.h"
#include "led_control.h"
#include "log.h"
#include "storage.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>

static HTTPClient httpQuota;

static void parseProvider(JsonObject obj, ProviderQuota &pq) {
    const char *p = obj["p"] | "";
    strncpy(pq.provider, p, sizeof(pq.provider) - 1);
    pq.provider[sizeof(pq.provider) - 1] = '\0';
    pq.entryCount = 0;

    // Add entries in the exact order expected by the UI labels for each provider.
    // Missing values are kept as -1/empty reset so drawing code can show blanks.
    auto addEntry = [&](int pct, const char *reset) {
        if (pq.entryCount >= 3) return;
        pq.entries[pq.entryCount].pct = pct;
        const char *r = reset ? reset : "";
        strncpy(pq.entries[pq.entryCount].reset, r, sizeof(QuotaEntry::reset) - 1);
        pq.entries[pq.entryCount].reset[sizeof(QuotaEntry::reset) - 1] = '\0';
        pq.entryCount++;
    };

    if (strcmp(p, "gemini") == 0) {
        addEntry(obj["gm"] | -1, obj["gmr"] | "");
        addEntry(obj["cl"] | -1, obj["clr"] | "");
    } else if (strcmp(p, "codex") == 0) {
        addEntry(obj["h5"] | -1, obj["h5r"] | "");
        addEntry(obj["w"]  | -1, obj["wr"]  | "");
    } else if (strcmp(p, "zhipu") == 0) {
        addEntry(obj["h5"]  | -1, obj["h5r"]  | "");
        addEntry(obj["w"]   | -1, obj["wr"]   | "");
        addEntry(obj["mcp"] | -1, obj["mcpr"] | "");
    }
}

bool fetchAllQuota(AllQuotaData &out) {
    memset(&out, 0, sizeof(AllQuotaData));
    if (cfgQuotaUrl.length() == 0) {
        LOGE("Quota URL not configured");
        return false;
    }
    String url = cfgQuotaUrl;

    HttpLedGuard httpLed;
    httpQuota.setTimeout(CONSOLE_TIMEOUT);
    httpQuota.begin(url);

    int code = httpQuota.GET();
    if (code != 200) {
        LOGW("Quota fetch failed: HTTP %d", code);
        httpQuota.end();
        return false;
    }

    String resp = httpQuota.getString();
    httpQuota.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        LOGE("Quota parse error: %s", err.c_str());
        return false;
    }

    // Response is an array of compact provider objects. Limit to the three panels
    // the e-ink UI can display and keep the struct RTC-friendly.
    JsonArray arr = doc.as<JsonArray>();
    int idx = 0;
    for (JsonObject obj : arr) {
        if (idx >= 3) break;
        memset(&out.providers[idx], 0, sizeof(ProviderQuota));
        for (int i = 0; i < 3; i++) out.providers[idx].entries[i].pct = -1;
        parseProvider(obj, out.providers[idx]);
        idx++;
    }

    out.valid = true;
    LOGI("Quota OK, providers=%d", idx);
    return true;
}

bool fetchProviderQuota(const char *provider, ProviderQuota &out) {
    if (cfgQuotaUrl.length() == 0) {
        LOGE("Quota URL not configured");
        return false;
    }
    String url = cfgQuotaUrl + "?provider=" + provider;

    HttpLedGuard httpLed;
    httpQuota.setTimeout(CONSOLE_TIMEOUT);
    httpQuota.begin(url);

    int code = httpQuota.GET();
    if (code != 200) {
        LOGW("Quota fetch failed: HTTP %d", code);
        httpQuota.end();
        return false;
    }

    String resp = httpQuota.getString();
    httpQuota.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        LOGE("Quota parse error: %s", err.c_str());
        return false;
    }

    memset(&out, 0, sizeof(ProviderQuota));
    for (int i = 0; i < 3; i++) out.entries[i].pct = -1;
    parseProvider(doc.as<JsonObject>(), out);
    LOGI("Quota OK, provider=%s entries=%d", out.provider, out.entryCount);
    return true;
}
