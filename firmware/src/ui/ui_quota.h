#ifndef EINK_UI_QUOTA_GX_H
#define EINK_UI_QUOTA_GX_H

#include <Arduino.h>

#include "../quota.h"

#define PAGE_OVERVIEW 0
#define PAGE_GOOGLE   1
#define PAGE_CODEX    2
#define PAGE_ZHIPU    3
#define PAGE_DEVICE   4
#define PAGE_COUNT    5

// Draw one of the five device pages. Callers pass already-fetched/cached data;
// this function never performs WiFi or HTTP work, so it is safe after WiFi off.
// usePartial is a request, not a guarantee: the display layer falls back when the
// selected panel cannot do partial refresh safely.
bool uiDrawPage(int pageIdx, const AllQuotaData &quota,
                float batteryV, int batteryPct,
                const char *ip, const char *mac,
                const char *wakeStr, int rssi, int freeHeap,
                int sleepMin, const char *firmware,
                bool usePartial = false);

// Standalone system pages used before or outside the normal quota dashboard.
void uiShowSetupScreen(const char *apName);
void uiShowError(const char *msg);
void uiShowSleepPrompt(int sleepMinutes);
void uiShowConnectionError(const char *errorType, const char *detail, int nextRetryMin);
void uiShowColorTest();

// Controls the bottom status badge: true -> [ON] during manual interaction,
// false -> [Zz] before automatic/deep-sleep presentation.
extern bool uiInteractiveActive;

#endif // EINK_UI_QUOTA_GX_H
