#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "display/display_gxepd2.h"
#include "ui/ui_quota.h"
#include "battery.h"
#include "button.h"
#include "power.h"
#include "wifi_manager.h"
#include "storage.h"
#include "portal.h"
#include "http_client.h"
#include "quota.h"
#include "led_control.h"
#include "log.h"
#include <ArduinoJson.h>

#if __has_include("firmware_version.h")
#include "firmware_version.h"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "local-" __DATE__ "-" __TIME__
#endif

// RTC memory survives ESP32 deep sleep, but is cleared by power loss or hard reset.
// Keep only small data that avoids unnecessary network/display work after timer wake.
RTC_DATA_ATTR AllQuotaData rtcQuota = {};
RTC_DATA_ATTR int rtcLastBatteryPct = -1;
RTC_DATA_ATTR int rtcLastSleepMin = -1;
RTC_DATA_ATTR char rtcLastDateStr[16] = {};
RTC_DATA_ATTR bool rtcLastInteractiveActive = false;
RTC_DATA_ATTR bool rtcDisplayValid = false;
RTC_DATA_ATTR int rtcWifiFailCount = 0;
RTC_DATA_ATTR time_t rtcLastTimeSync = 0;
RTC_DATA_ATTR int rtcScheduleSleepMin = -1;
RTC_DATA_ATTR bool rtcLastScreenWasError = false;

static unsigned long portalStartAt = 0;
const unsigned long PORTAL_TIMEOUT_MS = 5 * 60 * 1000ULL;
static const int TIME_SYNC_INTERVAL_SEC = 4 * 60 * 60;       // timer wake: sync at most once every 4 hours
static const int AUTO_DISPLAY_TIMEOUT_MS = 2000;            // auto wake only leaves the new page visible briefly
static const int LOW_BATTERY_PORTAL_TIMEOUT_MS = 2 * 60 * 1000; // shorten AP lifetime when battery is low

// State machine
enum class State : uint8_t {
    BOOT,
    PORTAL,
    ONLINE,
    DISPLAYING,
};

static State state = State::BOOT;
static unsigned long displayStartAt = 0;
static bool isInteractiveMode = false;
static int currentPage = 0;

// Cached display data for current ONLINE cycle
static AllQuotaData curQuota = {};
static float curBatteryV = 0;
static int curBatteryPct = 0;
static String curIP;
static String curMAC;
static const char *curWakeStr = "boot";
static int curRSSI = 0;
static int curFreeHeap = 0;
static char curFirmware[32] = FIRMWARE_VERSION;
static unsigned long awakeStartMs = 0;

static int validPageOrDefault(int page) {
    if (page < 0 || page >= PAGE_COUNT) return PAGE_OVERVIEW;
    return page;
}

static const ProviderQuota *findQuotaProvider(const AllQuotaData &data, const char *provider) {
    for (int i = 0; i < 3; i++) {
        if (strcmp(data.providers[i].provider, provider) == 0) return &data.providers[i];
    }
    return nullptr;
}

static bool sameProviderQuota(const char *provider) {
    const ProviderQuota *cur = findQuotaProvider(curQuota, provider);
    const ProviderQuota *last = findQuotaProvider(rtcQuota, provider);
    if (cur == nullptr || last == nullptr) return cur == last;
    return memcmp(cur, last, sizeof(ProviderQuota)) == 0;
}

static bool sameCurrentPageQuota() {
    switch (currentPage) {
    case PAGE_GOOGLE:
        return sameProviderQuota("gemini");
    case PAGE_CODEX:
        return sameProviderQuota("codex");
    case PAGE_ZHIPU:
        return sameProviderQuota("zhipu");
    case PAGE_OVERVIEW:
    default:
        return memcmp(&curQuota, &rtcQuota, sizeof(AllQuotaData)) == 0;
    }
}

static void enterPortalMode() {
    String mac = WiFi.macAddress();
    String apName = String(AP_NAME_PREFIX) + "-" + mac.substring(mac.length() - 5);
    apName.replace(":", "");

    // Portal owns WiFi in AP+STA mode. Turn off any previous STA session first
    // so failed auto-connect state does not leak into SoftAP configuration.
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    uiShowSetupScreen(apName.c_str());
    startCaptivePortal();
    portalStartAt = millis();
    state = State::PORTAL;
    buttonIgnoreUntilRelease();
}

static void enterDeepSleepWithDisplay(int minutes) {
    // Every sleep path must leave high-current peripherals quiescent. The display
    // keeps its image without power, LED is forced off, and WiFi radio is stopped.
    rtcScheduleSleepMin = minutes;  // store actual sleep interval in RTC (for custom schedule)
    displaySleep();
    ledForceOff();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    enterDeepSleep(minutes);
}

// Get date bucket for deciding whether the e-ink page is still visually current.
// Format: "M-D-W-period" where period splits the day into coarse Chinese labels.
static void getDateStr(char *out, int len) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *t = localtime(&tv.tv_sec);
    int period = 5; // 18:00-23:59 晚上
    if (t->tm_hour < 6) {
        period = 0; // 凌晨
    } else if (t->tm_hour < 8) {
        period = 1; // 清晨
    } else if (t->tm_hour < 12) {
        period = 2; // 上午
    } else if (t->tm_hour < 13) {
        period = 3; // 中午
    } else if (t->tm_hour < 18) {
        period = 4; // 下午
    }
    snprintf(out, len, "%d-%d-%d-%d", t->tm_mon + 1, t->tm_mday, t->tm_wday, period);
}

// Draw current page using cached data.
// forceRefresh=true always refreshes (button page switch).
// forceRefresh=false skips epdDisplay when data hasn't changed.
// Returns true when a hardware refresh was performed.
static bool showCurrentPage(bool forceRefresh = false) {
    char dateStr[16];
    getDateStr(dateStr, sizeof(dateStr));

    bool needRefresh = true;
    if (!forceRefresh && rtcDisplayValid && currentPage == rtcLastPage) {
        if (currentPage == PAGE_DEVICE) {
            // Device page always refreshes (minute-precise time)
        } else {
            bool batterySame = true;
            if (rtcDisplayValid) {
                int diff = abs(curBatteryPct - rtcLastBatteryPct);
                // Only trigger update if change > 2%, or battery hit 100% (full), or low battery <= 20%
                if (diff > 2 || curBatteryPct == 100 || curBatteryPct <= 20) {
                    batterySame = false;
                }
            } else {
                batterySame = false;
            }

            bool dataSame = (batterySame &&
                             cfgSleepMin == rtcLastSleepMin &&
                             uiInteractiveActive == rtcLastInteractiveActive &&
                             strcmp(dateStr, rtcLastDateStr) == 0 &&
                             sameCurrentPageQuota());
            if (dataSame) {
                LOGI("Display unchanged, skipping refresh");
                needRefresh = false;
            } else if (cfgSleepMin != rtcLastSleepMin) {
                LOGI("Display refresh needed: sleep_min changed %d -> %d", rtcLastSleepMin, cfgSleepMin);
            } else if (uiInteractiveActive != rtcLastInteractiveActive) {
                LOGI("Display refresh needed: activity badge changed %d -> %d", rtcLastInteractiveActive, uiInteractiveActive);
            }
        }
    }

    if (needRefresh) {
        // Decide whether to use partial refresh for extremely fast visual response
        bool usePartial = false;
        if (displayCaps().supportsPartial) {
            if (forceRefresh) {
                // Interactive manual page switching: smooth partial refresh (0.3s)
                usePartial = true;
            } else if (currentPage == PAGE_DEVICE) {
                // Device page auto-updates time (minute-precise): partial refresh
                usePartial = true;
            }
        }

        uiDrawPage(currentPage, curQuota,
                   curBatteryV, curBatteryPct,
                   curIP.c_str(), curMAC.c_str(),
                   curWakeStr, curRSSI, curFreeHeap,
                   cfgSleepMin, curFirmware,
                   usePartial);

        // ONLY update RTC snapshots when an actual hardware refresh took place!
        // This prevents the "boiling frog" bug where incremental 1% drops are never drawn.
        rtcQuota = curQuota;
        rtcLastPage = currentPage;
        rtcLastBatteryPct = curBatteryPct;
        rtcLastSleepMin = cfgSleepMin;
        rtcLastInteractiveActive = uiInteractiveActive;
        strncpy(rtcLastDateStr, dateStr, sizeof(rtcLastDateStr));
        rtcLastDateStr[sizeof(rtcLastDateStr) - 1] = '\0';
        rtcDisplayValid = true;
    }
    return needRefresh;
}

static ProviderQuota* findOrCreateProviderSlot(AllQuotaData &q, const char *provider) {
    // 1. Try to find existing slot
    for (int i = 0; i < 3; i++) {
        if (strcmp(q.providers[i].provider, provider) == 0) {
            return &q.providers[i];
        }
    }
    // 2. Find empty slot
    for (int i = 0; i < 3; i++) {
        if (q.providers[i].provider[0] == '\0') {
            strncpy(q.providers[i].provider, provider, sizeof(q.providers[i].provider) - 1);
            q.providers[i].provider[sizeof(q.providers[i].provider) - 1] = '\0';
            return &q.providers[i];
        }
    }
    // 3. Fallback: overwrite first slot
    strncpy(q.providers[0].provider, provider, sizeof(q.providers[0].provider) - 1);
    q.providers[0].provider[sizeof(q.providers[0].provider) - 1] = '\0';
    return &q.providers[0];
}

static int getBackoffSleepMin(int failCount) {
    if (failCount <= 1) return 5;
    // Linear progression: 5, 7.5, 10, 12.5, 15... capped at 180 minutes (3 hours)
    float minutes = 5.0f + 2.5f * (failCount - 1);
    if (minutes > 180.0f) return 180;
    return (int)minutes;
}

static int lowBatterySleepMin() {
    // Low battery never changes request frequency directly. It only lengthens the
    // next sleep once the current wake cycle has no more required network work.
    if (curBatteryPct <= 5) return 1440;
    if (curBatteryPct <= 10) return 720;
    if (curBatteryPct <= 20) return max(cfgSleepMin, 60);
    return cfgSleepMin;
}

static bool shouldSyncTimeNow() {
    esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    struct timeval tv;
    gettimeofday(&tv, NULL);

    // Button wake is an interactive cache-browsing path. Do not spend power on
    // time sync unless the console explicitly sends a sync_time task later.
    if (wakeCause == ESP_SLEEP_WAKEUP_GPIO) return false;
    // First power-on and manual reset should get a fresh clock immediately.
    if (!isTimerWakeup()) return true;
    // Timer wake keeps the RTC clock warm, but still repairs invalid time and
    // refreshes against the console at a bounded 4-hour cadence.
    if (tv.tv_sec < 1700000000) return true;
    if (rtcLastTimeSync == 0) return true;
    return (tv.tv_sec - rtcLastTimeSync) >= TIME_SYNC_INTERVAL_SEC;
}

static unsigned long portalTimeoutMs() {
    return curBatteryPct <= 20 ? LOW_BATTERY_PORTAL_TIMEOUT_MS : PORTAL_TIMEOUT_MS;
}

// ═══════════════════════════════════════════════════════════
// setup()
// ═══════════════════════════════════════════════════════════

void setup() {
    awakeStartMs = millis();
    buttonInit();
    bool isBootHeldEarly = buttonHeldAtBoot();

    Serial.begin(115200);
    delay(3000);
    LOGI("=== eink fw %s built " __DATE__ " " __TIME__ " ===", curFirmware);

    // Capture button state late (in case they pressed it during the 3-second serial delay)
    bool isBootHeldLate = (digitalRead(PIN_CFG_BTN) == LOW);

    setenv("TZ", "CST-8", 1);
    tzset();

    batteryInit();
    ledControlInit();
    displayBegin();

    // Wake reason determines whether this cycle is automatic or interactive.
    // Automatic wakes should finish quickly and sleep; button wakes keep the
    // display active long enough for page switching without reconnecting later.
    bool isButtonWake = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO);
    if (isTimerWakeup()) {
        LOGI("Wakeup: timer");
        currentPage = validPageOrDefault(rtcLastPage);
        isInteractiveMode = false;
        uiInteractiveActive = false; // 定时唤醒：全自动流程，休眠态
        ledBlinkN(3, 80); // 规范#1: 唤醒瞬时确认 - 极速快闪3次(80ms)
    } else if (isButtonWake) {
        LOGI("Wakeup: button");
        currentPage = validPageOrDefault(rtcLastPage);
        isInteractiveMode = true;
        uiInteractiveActive = true;  // 按键唤醒：活跃交互，开启运行态
        ledBlinkN(3, 80); // 规范#1: 唤醒瞬时确认 - 极速快闪3次(80ms)
    } else {
        LOGI("Wakeup: boot/reset");
        currentPage = PAGE_OVERVIEW;
        isInteractiveMode = false;
        rtcQuota.valid = false;
        rtcDisplayValid = false;
        uiInteractiveActive = false; // 重新上电/复位：默认初始休眠态
        ledBlinkN(3, 80); // 规范#1: 唤醒瞬时确认 - 极速快闪3次(80ms)
    }

    // Read battery before network decisions so low-battery paths can shorten AP
    // timeout and stretch sleep without paying for WiFi unnecessarily.
    curBatteryV = readBatteryVoltage();
    curBatteryPct = batteryPercent(curBatteryV);
    LOGI("Battery: %.2fV (%d%%)", curBatteryV, curBatteryPct);

    // Holding the key through boot is the explicit local escape hatch into
    // configuration, independent of saved WiFi state or previous failures.
    if (isBootHeldEarly || isBootHeldLate) {
        LOGI("Button held at boot (Early=%d, Late=%d) -> portal", isBootHeldEarly, isBootHeldLate);
        loadConfig();
        enterPortalMode();
        return;
    }

    loadConfig();

    // Restore schedule-driven interval from RTC (survives deep sleep, cleared on cold boot).
    // On cold boot rtcScheduleSleepMin == -1, so NVS default is used until first heartbeat.
    if (rtcScheduleSleepMin > 0) {
        cfgSleepMin = rtcScheduleSleepMin;
    }

    if (!isInteractiveMode && curBatteryPct <= 10 && rtcDisplayValid) {
        // At very low battery an automatic wake does not burn power on WiFi when
        // the screen already has a valid cached page. Manual button wake still
        // allows the user to inspect pages and force portal if needed.
        int sleepMin = lowBatterySleepMin();
        LOGW("Low battery %d%%, skip network and sleep %d min", curBatteryPct, sleepMin);
        enterDeepSleepWithDisplay(sleepMin);
        return;
    }

    // No WiFi config → portal
    if (cfgSSID.length() == 0) {
        LOGI("No WiFi config -> portal");
        enterPortalMode();
        return;
    }

    // Connect WiFi
    // 规范#2: 联网慢闪由 wifi_manager.cpp 中的 300ms 翻转自动驱动
    if (!connectWiFi()) {
        if (!isInteractiveMode && cfgSSID.length() > 0) {
            LOGW("WiFi failed during auto wake, entering silent backoff sleep");
            rtcWifiFailCount++;
            int backoffMin = max(getBackoffSleepMin(rtcWifiFailCount), lowBatterySleepMin());
            
            if (rtcWifiFailCount == 1 || !rtcDisplayValid) {
                rtcLastScreenWasError = true;
                uiShowConnectionError("WiFi Offline", "Could not connect to router", backoffMin);
            }
            enterDeepSleepWithDisplay(backoffMin);
            return;
        } else {
            LOGW("WiFi failed, transitioning to portal");
            ledActivityBegin(); // 规范#7: 网络故障警告 - 长亮2秒
            delay(2000);
            ledActivityEnd();
            enterPortalMode();
            return;
        }
    }

    // Enter ONLINE state
    state = State::ONLINE;
    resetRetryCount();
}

// ═══════════════════════════════════════════════════════════
// loop()
// ═══════════════════════════════════════════════════════════

void loop() {
    if (state == State::PORTAL) {
        handlePortalClients();

        // Triple-Blink non-blocking loop (1.6s period)
        unsigned long now = millis();
        int phase = (now - portalStartAt) % 1600;
        if (phase < 80 || (phase >= 160 && phase < 240) || (phase >= 320 && phase < 400)) {
            ledSetIdle(true);
        } else {
            ledSetIdle(false);
        }

        // 5-minute portal configuration timeout
        if (millis() - portalStartAt > portalTimeoutMs()) {
            LOGW("Portal timeout, entering power-saving deep sleep");
            
            rtcWifiFailCount++;
            int backoffMin = max(getBackoffSleepMin(rtcWifiFailCount), lowBatterySleepMin());
            
            // Render premium sleeping/recovery prompt screen
            uiShowSleepPrompt(backoffMin);
            
            // Enter deep sleep
            enterDeepSleepWithDisplay(backoffMin);
            return;
        }

        ButtonEvent evt = buttonPoll();
        if (evt == ButtonEvent::LONG_PRESS) {
            LOGI("Portal: long press -> restart");
            ESP.restart();
        }
        delay(5);
        return;
    }

    // ONLINE state owns the full network phase: optional time sync, quota fetch,
    // heartbeat, task fetch and task reporting. WiFiSessionGuard powers the radio
    // off before display/interaction so cached page switching does not keep WiFi on.
    if (state == State::ONLINE) {
        WiFiSessionGuard wifiSession;

        // Time policy: boot/reset syncs immediately, timer wake syncs every 4h,
        // button wake skips automatic sync, sync_time task always syncs now.
        if (shouldSyncTimeNow()) {
            unsigned long syncStart = millis();
            LOGI("-- Auto time sync --");
            if (syncTimeFromServer()) {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                rtcLastTimeSync = tv.tv_sec;
            }
            LOGI("Time sync elapsed=%lums", millis() - syncStart);
        }

        curIP = WiFi.localIP().toString();
        curMAC = WiFi.macAddress();
        curMAC.replace(":", "");
        curRSSI = (int)WiFi.RSSI();
        curFreeHeap = (int)ESP.getFreeHeap();

        // Determine wake reason
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
            curWakeStr = "button";
        } else if (isTimerWakeup()) {
            curWakeStr = "timer";
        } else {
            curWakeStr = "boot";
        }

        // 1. Fetch quota
        LOGI("-- Fetch quota --");
        unsigned long quotaStart = millis();
        memset(&curQuota, 0, sizeof(AllQuotaData));
        bool fetchOk = false;
        int httpRetryMax = isInteractiveMode ? 3 : 2;
        for (int retry = 1; retry <= httpRetryMax; retry++) {
            fetchOk = fetchAllQuota(curQuota);
            if (fetchOk) break;
            LOGW("Quota attempt %d/%d failed", retry, httpRetryMax);
            if (retry < httpRetryMax) delay(1000);
        }
        LOGI("Quota phase elapsed=%lums", millis() - quotaStart);

        // 2. Send heartbeat (console mode only)
        int sleepMin = -1;
        if (fetchOk && hasConsole()) {
            LOGI("-- Send heartbeat --");
            unsigned long heartbeatStart = millis();
            for (int retry = 1; retry <= httpRetryMax; retry++) {
                sleepMin = sendHeartbeat(curBatteryV, curBatteryPct, curIP.c_str(), curWakeStr,
                                             (int)(millis() / 1000), curFreeHeap, curRSSI, cfgSSID.c_str(), curFirmware);
                if (sleepMin >= 0) break;
                LOGW("Heartbeat attempt %d/%d failed", retry, httpRetryMax);
                if (retry < httpRetryMax) delay(1000);
            }
            LOGI("Heartbeat phase elapsed=%lums", millis() - heartbeatStart);
        } else if (!hasConsole()) {
            LOGI("Standalone mode, skipping heartbeat");
        }

        // Standalone mode: only quota failure counts.
        // Console mode: quota or heartbeat failure triggers backoff.
        bool networkFailed = !fetchOk || (hasConsole() && sleepMin < 0);
        if (networkFailed) {
            rtcWifiFailCount++;
            int backoffMin = max(getBackoffSleepMin(rtcWifiFailCount), lowBatterySleepMin());
            
            // Restore last successful quota data from RTC so that page browsing still works
            if (rtcQuota.valid) {
                curQuota = rtcQuota;
            }
            rtcLastScreenWasError = true;

            const char *errorDetail = !fetchOk ? "Quota server unreachable" : "Console server offline";

            if (!isInteractiveMode && cfgSSID.length() > 0) {
                LOGW("Network sync failed, entering silent backoff sleep");
                
                if (rtcWifiFailCount == 1 || !rtcDisplayValid) {
                    uiShowConnectionError("Sync Failed", errorDetail, backoffMin);
                }
                
                // Keep the previous e-ink screen content cleanly and hibernate
                wifiSession.stopNow();
                enterDeepSleepWithDisplay(backoffMin);
                return;
            } else {
                LOGW("Network sync failed during interactive wake, showing connection error");
                ledActivityBegin(); // 规范#7: 网络故障警告 - 长亮2秒
                delay(2000);
                ledActivityEnd();

                // Show connection error screen but do NOT force transition to portal!
                // Keep WiFi off to save power, and transition to DISPLAYING so user can browse cache.
                uiShowConnectionError("Sync Failed", errorDetail, backoffMin);
                wifiSession.stopNow();

                state = State::DISPLAYING;
                displayStartAt = millis();
                return;
            }
        }

        // Reset failure count upon full network success
        rtcWifiFailCount = 0;
        rtcLastScreenWasError = false;

        if (sleepMin > 0) {
            if (lastHeartbeatPersist) {
                if (sleepMin != cfgSleepMin) {
                    LOGI("Console suggests sleep_min=%d (persist), updating", sleepMin);
                    saveSleepMin(sleepMin);
                }
            } else {
                LOGI("Console suggests sleep_min=%d (RTC only)", sleepMin);
                cfgSleepMin = sleepMin;  // update runtime only, no flash write
            }
        }

        // 3. Fetch & execute pending tasks (console mode only)
        if (hasConsole()) {
            LOGI("-- Fetch tasks --");
            unsigned long tasksStart = millis();
            ConsoleTask tasks[8];
            int taskCount = fetchPendingTasks(curMAC.c_str(), tasks, 8);
            LOGI("Tasks phase elapsed=%lums", millis() - tasksStart);

            // 4. Execute tasks
            for (int i = 0; i < taskCount; i++) {
                LOGI("Task %d: %s", tasks[i].id, tasks[i].type);
                bool ok = true;

                if (strcmp(tasks[i].type, "set_sleep") == 0) {
                    JsonDocument pdoc;
                    deserializeJson(pdoc, tasks[i].params);
                    int minutes = pdoc["minutes"] | 0;
                    if (minutes > 0) {
                        saveSleepMin(minutes);
                        LOGI("Task set_sleep -> %d min", minutes);
                    } else {
                        ok = false;
                    }
                }
                else if (strcmp(tasks[i].type, "sync_time") == 0) {
                    ok = syncTimeFromServer();
                    if (ok) {
                        struct timeval tv;
                        gettimeofday(&tv, NULL);
                        rtcLastTimeSync = tv.tv_sec;
                    }
                }
                else if (strcmp(tasks[i].type, "restart") == 0) {
                    reportTaskResult(tasks[i].id, true, "{}");
                    LOGW("Task restart -> restarting");
                    ledForceOff();
                    delay(200);
                    ESP.restart();
                }
                else if (strcmp(tasks[i].type, "switch_page") == 0) {
                    JsonDocument pdoc;
                    deserializeJson(pdoc, tasks[i].params);
                    int page = pdoc["page"] | -1;
                    if (page >= 0 && page < PAGE_COUNT) {
                        currentPage = page;
                        LOGI("Task switch_page -> %d", page);
                    } else {
                        ok = false;
                        LOGW("Task switch_page: invalid page %d", page);
                    }
                }
                else {
                    LOGW("Unknown task type: %s", tasks[i].type);
                    ok = false;
                }

                reportTaskResult(tasks[i].id, ok, "{}");
            }
        } else {
            LOGI("Standalone mode, skipping tasks");
        }

        // From here onward display and button handling use only cached data.
        // This is why manually switching quota pages after display does not need WiFi.
        wifiSession.stopNow();
        // 5. Print device time
        struct timeval tvNow;
        gettimeofday(&tvNow, NULL);
        struct tm *tmNow = localtime(&tvNow.tv_sec);
        LOGI("Device time: %04d-%02d-%02d %02d:%02d:%02d",
                       1900 + tmNow->tm_year, 1 + tmNow->tm_mon, tmNow->tm_mday,
                       tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec);

        // 6. Display current page
        unsigned long displayRefreshStart = millis();
        bool refreshed = showCurrentPage(); // 规范#4: 刷新期间驱动层自动常亮LED
        LOGI("Display refresh phase elapsed=%lums", millis() - displayRefreshStart);

        if (!refreshed && !isInteractiveMode) {
            // If RTC snapshot says the visible e-ink content is already current,
            // an automatic wake can sleep immediately after network work.
            LOGI("No display refresh needed, sleeping immediately");
            LOGI("Awake total=%lums", millis() - awakeStartMs);
            enterDeepSleepWithDisplay(lowBatterySleepMin());
            return;
        }

        if (refreshed) ledBlinkN(2, 150); // 规范#3: 联网成功&可交互 - 高级双闪(150ms)

        state = State::DISPLAYING;
        displayStartAt = millis();
        return;
    }

    // DISPLAYING state: interactive or auto-sleep
    if (state == State::DISPLAYING) {
        unsigned long timeout = isInteractiveMode ? INTERACTIVE_TIMEOUT : AUTO_DISPLAY_TIMEOUT_MS;
        unsigned long elapsed = millis() - displayStartAt;

        ButtonEvent evt = buttonPoll();
        if (evt == ButtonEvent::SHORT_PRESS) {
            uiInteractiveActive = true;
            isInteractiveMode = true;
            rtcLastScreenWasError = false; // User dismissed the error screen by switching page
            // Next page
            currentPage = (currentPage + 1) % PAGE_COUNT;
            LOGI("Button page -> %d", currentPage);
            buttonLock(1000); // self-lock 1s
            showCurrentPage(true); // 驱动层自动 LED 亮灭：刷新开始亮，结束灭
            displayStartAt = millis(); // reset timeout
            elapsed = 0;
        } else if (evt == ButtonEvent::LONG_PRESS) {
            LOGI("Button long press -> manual data refresh");
            
            // 1. Show activity LED
            ledActivityBegin();

            // 2. Connect WiFi
            WiFiSessionGuard wifiSession;
            if (!connectWiFi()) {
                LOGW("Manual refresh: WiFi connection failed");
                ledActivityEnd();
                // Blink LED for 2 seconds to show connection failed
                ledBlinkN(1, 2000); 
                displayStartAt = millis();
                return;
            }

            // 3. Update battery and network parameters
            curBatteryV = readBatteryVoltage();
            curBatteryPct = batteryPercent(curBatteryV);
            curIP = WiFi.localIP().toString();
            curRSSI = (int)WiFi.RSSI();
            curFreeHeap = (int)ESP.getFreeHeap();

            // 4. Fetch the respective data based on currentPage
            bool fetchOk = false;
            if (currentPage == PAGE_OVERVIEW) {
                LOGI("Manual refresh: fetch all quota");
                fetchOk = fetchAllQuota(curQuota);
            } else if (currentPage == PAGE_GOOGLE) {
                LOGI("Manual refresh: fetch google quota");
                fetchOk = fetchProviderQuota("gemini", *findOrCreateProviderSlot(curQuota, "gemini"));
            } else if (currentPage == PAGE_CODEX) {
                LOGI("Manual refresh: fetch codex quota");
                fetchOk = fetchProviderQuota("codex", *findOrCreateProviderSlot(curQuota, "codex"));
            } else if (currentPage == PAGE_ZHIPU) {
                LOGI("Manual refresh: fetch zhipu quota");
                fetchOk = fetchProviderQuota("zhipu", *findOrCreateProviderSlot(curQuota, "zhipu"));
            } else if (currentPage == PAGE_DEVICE) {
                LOGI("Manual refresh: sync time");
                fetchOk = syncTimeFromServer();
                if (hasConsole()) {
                    int sleepMin = sendHeartbeat(curBatteryV, curBatteryPct, curIP.c_str(), "manual_refresh",
                                                 (int)(millis() / 1000), curFreeHeap, curRSSI, cfgSSID.c_str(), curFirmware);
                    fetchOk = fetchOk && (sleepMin >= 0);
                }
            }

            // 5. Update display if there is a difference
            if (fetchOk) {
                rtcWifiFailCount = 0;
                rtcLastScreenWasError = false;

                bool refreshed = showCurrentPage(false); // forceRefresh = false checks for differences!
                if (refreshed) {
                    LOGI("Manual refresh: data changed, display refreshed");
                    ledBlinkN(2, 150); // Success double-blink (150ms)
                } else {
                    LOGI("Manual refresh: data unchanged, skipped display refresh");
                    ledBlinkN(3, 80); // Triple-blink (80ms) to show "no change"
                }
            } else {
                LOGW("Manual refresh: data fetch failed");
                ledBlinkN(1, 1000); // 1-second blink to show fetch failure
            }

            wifiSession.stopNow();
            ledActivityEnd();
            displayStartAt = millis();
            return;
        } else if (evt == ButtonEvent::VERY_LONG_PRESS) {
            LOGI("Button very long press -> portal");
            enterPortalMode(); // 规范#4+#6: 配网屏刷新时驱动层自动亮灯，随后进入三闪循环
            return;
        }

        // Sleep after timeout
        if (elapsed > timeout) {
            LOGI("Display done (%s mode, %lums), sleeping %d min",
                          isInteractiveMode ? "interactive" : "auto",
                          elapsed, lowBatterySleepMin());
            
            // 仅在手动活跃交互结束后，将状态设为休眠并重新刷新屏幕（使底栏呈现 [Zz] 徽章），如果是错误屏幕则不刷新覆盖
            if (uiInteractiveActive && !rtcLastScreenWasError) {
                uiInteractiveActive = false;
                showCurrentPage(true);
            }
            // 规范#4: 若上方 showCurrentPage(true) 触发了刷新，驱动层已自动亮灭 LED
            LOGI("Awake total=%lums", millis() - awakeStartMs);
            enterDeepSleepWithDisplay(lowBatterySleepMin());
        }

        delay(50);
        return;
    }

    delay(50);
}
