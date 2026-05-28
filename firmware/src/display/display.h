#ifndef EINK_DISPLAY_H
#define EINK_DISPLAY_H

#include <Arduino.h>

enum class InkColor : uint8_t {
    White,
    Black,
    // Logical accent color. Three-color panels render this as red; black/white
    // panels degrade it to black in displayColor().
    Red,
};

struct DisplayCaps {
    int width;
    int height;
    // Capability flags let UI code choose safe refresh/color behavior without
    // knowing the concrete GxEPD2 driver type selected at compile time.
    bool hasRed;
    bool supportsPartial;
    bool supportsFastPartial;
    const char *model;
};

#endif // EINK_DISPLAY_H
