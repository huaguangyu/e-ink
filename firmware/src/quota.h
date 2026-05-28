#ifndef EINK_QUOTA_H
#define EINK_QUOTA_H

#include <Arduino.h>

// Single quota entry: label + percentage + reset time
struct QuotaEntry {
    int  pct;       // 0~100, -1 = not available
    char reset[12]; // "HH:MM" or "MM-DD HH:MM"
};

// Per-provider data
struct ProviderQuota {
    char provider[8]; // "gemini", "codex", "zhipu"
    QuotaEntry entries[3]; // max 3 entries per provider
    int  entryCount;
};

// All quota data (fitted in RTC memory)
struct AllQuotaData {
    ProviderQuota providers[3]; // gemini, codex, zhipu
    bool valid;                 // data loaded successfully
};

// Fetch all providers at once. The firmware intentionally keeps this as the
// existing direct quota API request and does not route it through console tasks.
bool fetchAllQuota(AllQuotaData &out);

// Fetch single provider helper for future detail-only refresh flows. Current page
// switching uses cached AllQuotaData so it works after WiFi has been turned off.
bool fetchProviderQuota(const char *provider, ProviderQuota &out);

#endif // EINK_QUOTA_H
