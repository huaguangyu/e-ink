#ifndef EINK_HTTP_CLIENT_H
#define EINK_HTTP_CLIENT_H

#include <Arduino.h>

// Task info from console
struct ConsoleTask {
    int  id;
    char type[16];   // set_sleep / sync_time / restart
    char params[64]; // JSON string
};

// Send heartbeat to console, returns suggested sleep_min (-1 on failure).
// The call also carries battery, wake reason, RSSI, heap and firmware version so the
// console can make remote scheduling decisions without a separate status request.
int  sendHeartbeat(float voltage, int pct, const char *ip,
                   const char *wake, int uptime_s, int free_heap,
                   int rssi, const char *ssid, const char *firmware);

// Fetch pending console tasks. Task execution remains in main.cpp because tasks
// may change sleep policy, set time, or restart the MCU.
int  fetchPendingTasks(const char *mac, ConsoleTask *out, int maxTasks);

// Report task execution result
void reportTaskResult(int taskId, bool success, const char *data);

// Sync time via NTP. Called by the automatic time-sync policy and by
// explicit sync_time tasks; successful callers update rtcLastTimeSync.
bool syncTimeFromServer();

// Set by sendHeartbeat(): true means server says persist sleep_min to flash.
extern bool lastHeartbeatPersist;

#endif // EINK_HTTP_CLIENT_H
