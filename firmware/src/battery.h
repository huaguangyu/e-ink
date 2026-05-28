#ifndef EINK_BATTERY_H
#define EINK_BATTERY_H

#include <Arduino.h>

// Initialize ADC for battery reading
void batteryInit();

// Read battery voltage (returns mapped voltage in V)
float readBatteryVoltage();

// Convert voltage to estimated percentage (0-100)
int batteryPercent(float voltage);

#endif // EINK_BATTERY_H
