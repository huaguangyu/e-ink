#ifndef EINK_PORTAL_H
#define EINK_PORTAL_H

#include <Arduino.h>

extern bool portalActive;
extern bool wifiConnected;

void startCaptivePortal();
void handlePortalClients();

#endif // EINK_PORTAL_H
