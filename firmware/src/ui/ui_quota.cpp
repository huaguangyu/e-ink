#include "ui_quota.h"

#include <sys/time.h>

#include "../config.h"
#include "../display/display_gxepd2.h"
#include "../fonts/inter_bold_28.h"
#include "../fonts/inter_regular_11.h"
#include "../fonts/inter_regular_14.h"
#include "../fonts/inter_semibold_18.h"
#include "../log.h"
#include "../storage.h"
#include "widgets_gx.h"

bool uiInteractiveActive = false;

static const int STATUS_H = 36;
static const int FOOTER_H = 22;
static const int CONTENT_Y = STATUS_H + 8;
static const int FOOTER_Y = H - FOOTER_H;

static uint16_t white() { return displayColor(InkColor::White); }
static uint16_t black() { return displayColor(InkColor::Black); }
static uint16_t red() { return displayColor(InkColor::Red); }

static const GFXfont *fontSmall() { return &InterRegular11pt7b; }
static const GFXfont *fontBody() { return &InterRegular14pt7b; }
static const GFXfont *fontTitle() { return &InterSemiBold18pt7b; }
static const GFXfont *fontBig() { return &InterBold28pt7b; }

static void formatDateCn(char *out, int len) {
    static const char *wd[] = {
        "\xe6\x97\xa5", "\xe4\xb8\x80", "\xe4\xba\x8c", "\xe4\xb8\x89",
        "\xe5\x9b\x9b", "\xe4\xba\x94", "\xe5\x85\xad",
    };
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm *t = localtime(&tv.tv_sec);
    const char *period = "\xe6\x99\x9a\xe4\xb8\x8a";  // 晚上
    if (t->tm_hour < 6) {
        period = "\xe5\x87\x8c\xe6\x99\xa8";  // 凌晨
    } else if (t->tm_hour < 8) {
        period = "\xe6\xb8\x85\xe6\x99\xa8";  // 清晨
    } else if (t->tm_hour < 12) {
        period = "\xe4\xb8\x8a\xe5\x8d\x88";  // 上午
    } else if (t->tm_hour < 13) {
        period = "\xe4\xb8\xad\xe5\x8d\x88";  // 中午
    } else if (t->tm_hour < 18) {
        period = "\xe4\xb8\x8b\xe5\x8d\x88";  // 下午
    }
    int n = snprintf(out, len, "%d\xe6\x9c\x88%d\xe6\x97\xa5 \xe5\x91\xa8",
                     t->tm_mon + 1, t->tm_mday);
    snprintf(out + n, len - n, "%s%s", wd[t->tm_wday], period);
}

static void getFullTime(char *out, int len) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm *t = localtime(&tv.tv_sec);
    snprintf(out, len, "%04d-%02d-%02d %02d:%02d",
             1900 + t->tm_year, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min);
}

static void drawHeader(Adafruit_GFX &d, const char *title) {
    d.fillRect(0, 0, W, 42, red());
    drawTextCenteredGx(d, fontTitle(), title, W / 2, 11, W - 28, displayCaps().hasRed ? white() : black());
    if (!displayCaps().hasRed) d.drawFastHLine(0, 42, W, black());
}

static void drawStatusBar(Adafruit_GFX &d, const char *title, int batteryPct) {
    uint16_t bg = displayCaps().hasRed ? red() : black();
    uint16_t fg = displayCaps().hasRed ? white() : white();
    d.fillRect(0, 0, W, STATUS_H, bg);

    const int margin = 10;
    const int gap = 8;
    char bat[8];
    snprintf(bat, sizeof(bat), "%d%%", batteryPct);

    char date[40];
    formatDateCn(date, sizeof(date));

    int titleW = textWidthGx(d, fontTitle(), title);
    int batW = textWidthGx(d, fontBody(), bat);
    int dateW = mixedDateWidthGx(d, fontSmall(), date);
    int leftEnd = margin + titleW + gap;
    int rightStart = W - margin - batW - gap;
    int dateX = (W - dateW) / 2;
    if (dateX < leftEnd) dateX = leftEnd;
    if (dateX + dateW > rightStart) dateX = rightStart - dateW;
    if (dateX < leftEnd || dateX < margin) {
        snprintf(date, sizeof(date), "");
        dateW = 0;
        dateX = W / 2;
    }

    const int baseline = STATUS_H / 2 + 8;

    d.setFont(fontTitle());
    d.setTextColor(fg);
    d.setCursor(margin, baseline);
    d.print(title);
    if (date[0]) drawMixedDateCnGx(d, fontSmall(), date, dateX, baseline - 10, fg);
    d.setFont(fontBody());
    d.setTextColor(fg);
    d.setCursor(W - margin - batW, baseline);
    d.print(bat);
}

static void drawFooter(Adafruit_GFX &d, int pageIdx) {
    drawDividerGx(d, 0, FOOTER_Y - 2, W, black());

    // 1. 左侧显示定时唤醒时长
    char next[16];
    snprintf(next, sizeof(next), "Sleep:%dm", cfgSleepMin);
    drawTextGx(d, fontSmall(), next, 12, FOOTER_Y + 5, black());

    // 2. 正中间居中显示动态交互/休眠胶囊图标
    const char *statusIcon = uiInteractiveActive ? "[ON]" : "[Zz]";
    drawTextCenteredGx(d, fontSmall(), statusIcon, W / 2, FOOTER_Y + 5, 80, black());

    // 3. 右侧显示当前页码
    char page[8];
    snprintf(page, sizeof(page), "%d/%d", pageIdx + 1, PAGE_COUNT);
    drawTextRightGx(d, fontSmall(), page, W - 12, FOOTER_Y + 5, black());
}

static void drawEmptyState(Adafruit_GFX &d, const char *msg) {
    drawTextCenteredGx(d, fontBody(), msg, W / 2, CONTENT_Y + 78, W - 32, black());
}

static void drawQuotaRow(Adafruit_GFX &d, const char *label, int pct, const char *resetTime, int y) {
    bool emphasis = pct >= 0 && pct < 30;
    const int rowH = 18;
    const int barH = 12;
    const int centerY = y + rowH / 2;
    const int baseline = y + rowH - 4;

    d.setFont(fontSmall());
    d.setTextColor(black());
    d.setCursor(8, baseline);
    if (label) d.print(label);

    drawProgressBarGx(d, 78, centerY - barH / 2, 170, barH, pct, emphasis, black(), red(), white());

    char pctStr[8];
    if (pct < 0) snprintf(pctStr, sizeof(pctStr), "--");
    else snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
    d.setFont(fontBody());
    d.setTextColor(emphasis ? red() : black());
    d.setCursor(260, baseline);
    d.print(pctStr);

    if (resetTime && resetTime[0]) {
        char fitted[20];
        fitTextGx(d, fontSmall(), resetTime, fitted, sizeof(fitted), W - 322 - 8);
        d.setFont(fontSmall());
        d.setTextColor(black());
        d.setCursor(322, baseline);
        d.print(fitted);
    }
}

static void drawLargeQuotaBlock(Adafruit_GFX &d, const char *label, int pct, const char *resetTime, int y) {
    bool emphasis = pct >= 0 && pct < 30;
    const int gap = 6;
    const int barH = 16;

    // 1. Draw label ("Gemini") with fontTitle() at topY = y
    drawTextGx(d, fontTitle(), label, 18, y, black());

    // 2. Measure label's actual baseline
    int16_t tx1, ty1;
    uint16_t tw, th;
    d.setFont(fontTitle());
    d.getTextBounds((char *)(label ? label : ""), 18, y, &tx1, &ty1, &tw, &th);
    int targetBaseline = y + (y - ty1);

    // 3. Format percentage string
    char pctStr[8];
    if (pct < 0) snprintf(pctStr, sizeof(pctStr), "--");
    else snprintf(pctStr, sizeof(pctStr), "%d%%", pct);

    // 4. Align percentage string's baseline to targetBaseline
    d.setFont(fontBig());
    int16_t px1, py1;
    uint16_t pw, ph;
    d.getTextBounds(pctStr, W - 18, targetBaseline, &px1, &py1, &pw, &ph);

    // The topY for drawTextRightGx is py1 (which is the yOffset-adjusted top bound)
    drawTextRightGx(d, fontBig(), pctStr, W - 18, py1, emphasis ? red() : black());

    // 5. Calculate barY using targetBaseline
    int barY = targetBaseline + gap;
    drawProgressBarGx(d, 18, barY, W - 36, barH, pct, emphasis, black(), red(), white());
    if (resetTime && resetTime[0]) {
        char reset[32];
        snprintf(reset, sizeof(reset), "Reset %s", resetTime);
        drawTextGx(d, fontBody(), reset, 18, barY + barH + gap, black());
    }
}

static const ProviderQuota *findProvider(const AllQuotaData &q, const char *provider) {
    for (int i = 0; i < 3; i++) {
        if (strcmp(q.providers[i].provider, provider) == 0) return &q.providers[i];
    }
    return nullptr;
}

static void drawPageOverview(Adafruit_GFX &d, const AllQuotaData &q) {
    if (!q.valid) {
        drawEmptyState(d, "No quota data");
        return;
    }

    const int sectionGap = 6;
    const int headerH = 24;
    const int rowH = 18;
    int totalH = 0;
    int activeSections = 0;
    for (int pi = 0; pi < 3; pi++) {
        if (q.providers[pi].entryCount == 0) continue;
        totalH += headerH + q.providers[pi].entryCount * rowH;
        activeSections++;
    }
    if (activeSections > 1) totalH += (activeSections - 1) * sectionGap;

    int contentH = FOOTER_Y - CONTENT_Y;
    int y = CONTENT_Y + max(0, (contentH - totalH) / 2);
    for (int pi = 0; pi < 3 && y < FOOTER_Y - rowH; pi++) {
        const ProviderQuota &pq = q.providers[pi];
        if (pq.entryCount == 0) continue;

        const char *sectionName = pq.provider;
        int nameIdx = -1;
        if (strcmp(pq.provider, "gemini") == 0) { sectionName = "Google"; nameIdx = 0; }
        else if (strcmp(pq.provider, "codex") == 0) { sectionName = "Codex"; nameIdx = 1; }
        else if (strcmp(pq.provider, "zhipu") == 0) { sectionName = "Zhipu"; nameIdx = 2; }

        static const char *names[][3] = {
            {"Gemini", "Claude", ""},
            {"5h", "Weekly", ""},
            {"5h", "Weekly", "MCP"},
        };

        drawTextGx(d, fontTitle(), sectionName, 8, y, black());
        y += headerH;
        for (int ei = 0; ei < pq.entryCount && y < FOOTER_Y - rowH; ei++) {
            const char *name = (nameIdx >= 0 && ei < 3) ? names[nameIdx][ei] : "";
            drawQuotaRow(d, name, pq.entries[ei].pct, pq.entries[ei].reset, y);
            y += rowH;
        }
        y += sectionGap;
    }
}

static void drawProviderDetail(Adafruit_GFX &d, const AllQuotaData &q,
                               const char *provider, const char *const *names, int nameCount) {
    const ProviderQuota *pq = findProvider(q, provider);
    if (!pq || pq->entryCount == 0) {
        drawEmptyState(d, "No quota data");
        return;
    }

    // Dynamically adjust block size and gap based on entry count to achieve premium visual rhythm
    int blockH = 76;
    int gap = 12;
    if (pq->entryCount >= 3) {
        blockH = 64;
        gap = 6;
    }

    int totalH = pq->entryCount * blockH + (pq->entryCount - 1) * gap;
    int contentH = FOOTER_Y - CONTENT_Y;
    int y = CONTENT_Y + max(0, (contentH - totalH) / 2);
    for (int i = 0; i < pq->entryCount && y < FOOTER_Y - 60; i++) {
        drawLargeQuotaBlock(d, (i < nameCount) ? names[i] : "", pq->entries[i].pct, pq->entries[i].reset, y);
        y += blockH + gap;
    }
}

static void drawDeviceLine(Adafruit_GFX &d, const char *label, const char *value, int y) {
    drawTextGx(d, fontBody(), label, 8, y, black());
    char fitted[56];
    fitTextGx(d, fontBody(), value, fitted, sizeof(fitted), W - 108 - 8);
    drawTextGx(d, fontBody(), fitted, 108, y, black());
}

static void drawPageDevice(Adafruit_GFX &d, float batteryV, int batteryPct,
                           const char *ip, const char *mac, const char *wakeStr,
                           int rssi, int freeHeap, int sleepMin, const char *firmware) {
    const int lineH = 20;
    char value[64];

    // Calculate total height of all elements to achieve dynamic vertical centering
    int activeLines = 8; // WiFi, Battery, Time, Wake, RSSI, Heap, Sleep, FW (always shown)
    if (ip && ip[0]) activeLines++;
    if (mac && mac[0]) activeLines++;
    int totalH = activeLines * lineH + 2; // Ordinary rows are 20px, battery progress bar takes 22px

    int contentH = FOOTER_Y - CONTENT_Y;
    int y = CONTENT_Y + max(0, (contentH - totalH) / 2);

    snprintf(value, sizeof(value), "%s", cfgSSID.c_str());
    drawDeviceLine(d, "WiFi", value, y); y += lineH;
    if (ip && ip[0]) { drawDeviceLine(d, "IP", ip, y); y += lineH; }
    if (mac && mac[0]) { drawDeviceLine(d, "MAC", mac, y); y += lineH; }

    snprintf(value, sizeof(value), "%.2fV  %d%%", batteryV, batteryPct);
    drawDeviceLine(d, "Battery", value, y); y += lineH;
    drawProgressBarGx(d, 8, y + 1, W - 16, 14, batteryPct, batteryPct < 30, black(), red(), white());
    y += 22;

    char timeStr[24];
    getFullTime(timeStr, sizeof(timeStr));
    drawDeviceLine(d, "Time", timeStr, y); y += lineH;
    drawDeviceLine(d, "Wake", wakeStr ? wakeStr : "", y); y += lineH;
    snprintf(value, sizeof(value), "%d dBm", rssi);
    drawDeviceLine(d, "RSSI", value, y); y += lineH;
    snprintf(value, sizeof(value), "%d KB", freeHeap / 1024);
    drawDeviceLine(d, "Heap", value, y); y += lineH;
    snprintf(value, sizeof(value), "%d min", sleepMin);
    drawDeviceLine(d, "Sleep", value, y); y += lineH;
    drawDeviceLine(d, "FW", firmware ? firmware : "", y);
}

bool uiDrawPage(int pageIdx, const AllQuotaData &quota,
                float batteryV, int batteryPct,
                const char *ip, const char *mac,
                const char *wakeStr, int rssi, int freeHeap,
                int sleepMin, const char *firmware,
                bool usePartial) {
    if (pageIdx < 0 || pageIdx >= PAGE_COUNT) pageIdx = PAGE_OVERVIEW;
    static const char *titles[] = {"AI Quota", "Google", "Codex", "Zhipu", "Device"};
    static const char *googleNames[] = {"Gemini", "Claude"};
    static const char *codexNames[] = {"5h", "Weekly"};
    static const char *zhipuNames[] = {"5h", "Weekly", "MCP"};

    if (usePartial) {
        displayPreparePartial(0, 0, W, H);
    } else {
        displayPrepareFull();
    }
    do {
        Adafruit_GFX &d = displayCanvas();
        displayClear();
        drawStatusBar(d, titles[pageIdx], batteryPct);
        switch (pageIdx) {
        case PAGE_GOOGLE: drawProviderDetail(d, quota, "gemini", googleNames, 2); break;
        case PAGE_CODEX:  drawProviderDetail(d, quota, "codex", codexNames, 2); break;
        case PAGE_ZHIPU:  drawProviderDetail(d, quota, "zhipu", zhipuNames, 3); break;
        case PAGE_DEVICE:
            drawPageDevice(d, batteryV, batteryPct, ip, mac, wakeStr, rssi, freeHeap, sleepMin, firmware);
            break;
        case PAGE_OVERVIEW:
        default:
            drawPageOverview(d, quota);
            break;
        }
        drawFooter(d, pageIdx);
    } while (displayNextPage());
    return true;
}

void uiShowSetupScreen(const char *apName) {
    displayPrepareFull();
    do {
        Adafruit_GFX &d = displayCanvas();
        displayClear();
        drawHeader(d, "SETUP WIFI");
        drawTextCenteredGx(d, fontBody(), "Connect to WiFi AP", W / 2, 74, W - 28, black());
        drawTextCenteredGx(d, fontBig(), apName ? apName : "", W / 2, 108, W - 28, black());
        d.fillRect(24, 166, W - 48, 2, black());
        drawTextCenteredGx(d, fontTitle(), "Open browser", W / 2, 188, W - 28, black());
        drawTextCenteredGx(d, fontBody(), "http://192.168.4.1", W / 2, 220, W - 28, black());
        drawTextCenteredGx(d, fontSmall(), "Configure WiFi, then restart", W / 2, 252, W - 28, black());
    } while (displayNextPage());
    LOGI("Setup screen shown: %s", apName ? apName : "");
}

void uiShowError(const char *msg) {
    displayPrepareFull();
    do {
        Adafruit_GFX &d = displayCanvas();
        displayClear();
        drawHeader(d, "ERROR");
        drawTextCenteredGx(d, fontTitle(), msg ? msg : "Unknown error", W / 2, 122, W - 28, black());
        drawTextCenteredGx(d, fontBody(), "Restart or enter setup mode", W / 2, 162, W - 28, black());
    } while (displayNextPage());
    LOGI("Error shown: %s", msg ? msg : "none");
}

void uiShowSleepPrompt(int sleepMinutes) {
    displayPrepareFull();
    do {
        Adafruit_GFX &d = displayCanvas();
        displayClear();
        drawHeader(d, "POWER SAVING");

        drawTextCenteredGx(d, fontTitle(), "Sleeping for Auto-Recovery", W / 2, 88, W - 28, black());

        char prompt[64];
        if (sleepMinutes >= 60) {
            snprintf(prompt, sizeof(prompt), "Next retry in %.1f hours", sleepMinutes / 60.0);
        } else {
            snprintf(prompt, sizeof(prompt), "Next retry in %d minutes", sleepMinutes);
        }
        drawTextCenteredGx(d, fontBig(), prompt, W / 2, 128, W - 28, black());

        d.fillRect(24, 180, W - 48, 2, black());

        drawTextCenteredGx(d, fontTitle(), "Need Setup WiFi?", W / 2, 202, W - 28, black());
        drawTextCenteredGx(d, fontSmall(), "Press physical button on the bottom", W / 2, 232, W - 28, black());
        drawTextCenteredGx(d, fontSmall(), "to wake up and configure anytime.", W / 2, 250, W - 28, black());
    } while (displayNextPage());
    LOGI("Sleep prompt screen shown: %d min", sleepMinutes);
}

void uiShowConnectionError(const char *errorType, const char *detail, int nextRetryMin) {
    displayPrepareFull();
    do {
        Adafruit_GFX &d = displayCanvas();
        displayClear();
        drawHeader(d, "CONNECTION ERROR");

        drawTextCenteredGx(d, fontTitle(), errorType ? errorType : "Sync Failed", W / 2, 88, W - 28, displayCaps().hasRed ? red() : black());
        drawTextCenteredGx(d, fontBody(), detail ? detail : "Console unreachable", W / 2, 122, W - 28, black());

        char prompt[64];
        if (nextRetryMin >= 60) {
            snprintf(prompt, sizeof(prompt), "Next retry in %.1f hours", nextRetryMin / 60.0);
        } else {
            snprintf(prompt, sizeof(prompt), "Next retry in %d minutes", nextRetryMin);
        }
        drawTextCenteredGx(d, fontBig(), prompt, W / 2, 165, W - 28, black());

        d.fillRect(24, 210, W - 48, 2, black());

        drawTextCenteredGx(d, fontSmall(), "Please check if smart control console is running", W / 2, 230, W - 28, black());
        drawTextCenteredGx(d, fontSmall(), "and the remote server address is configured correctly.", W / 2, 248, W - 28, black());
        drawTextCenteredGx(d, fontSmall(), "Press physical button to wake and retry.", W / 2, 266, W - 28, black());
    } while (displayNextPage());
    LOGI("Connection error screen shown: %s - %s, nextRetry=%d min", errorType, detail, nextRetryMin);
}

void uiShowColorTest() {
    displayPrepareFull();
    do {
        Adafruit_GFX &d = displayCanvas();
        displayClear();
        drawTextCenteredGx(d, fontTitle(), "GxEPD2 OK", W / 2, 48, W - 20, black());
        drawTextCenteredGx(d, fontBody(), displayCaps().model, W / 2, 92, W - 20, black());
        d.fillRect(40, 138, 130, 54, black());
        d.drawRect(230, 138, 130, 54, black());
        if (displayCaps().hasRed) d.fillRect(230, 138, 130, 54, red());
        drawTextCenteredGx(d, fontSmall(), displayCaps().supportsPartial ? "partial yes" : "partial no",
                           W / 2, 236, W - 20, black());
    } while (displayNextPage());
}
