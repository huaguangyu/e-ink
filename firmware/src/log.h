#ifndef EINK_LOG_H
#define EINK_LOG_H

#include <Arduino.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

inline const char *logBaseName(const char *path) {
    const char *base = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }
    return base;
}

#define LOG_LEVEL_PRINT(level, fmt, ...) \
    do { \
        Serial.printf("[%s %s:%d %s] " fmt "\n", level, logBaseName(__FILE__), __LINE__, __func__, ##__VA_ARGS__); \
    } while (0)

#if LOG_LEVEL >= 3
#define LOGI(fmt, ...) LOG_LEVEL_PRINT("I", fmt, ##__VA_ARGS__)
#else
#define LOGI(fmt, ...) do { } while (0)
#endif

#if LOG_LEVEL >= 2
#define LOGW(fmt, ...) LOG_LEVEL_PRINT("W", fmt, ##__VA_ARGS__)
#else
#define LOGW(fmt, ...) do { } while (0)
#endif

#if LOG_LEVEL >= 1
#define LOGE(fmt, ...) LOG_LEVEL_PRINT("E", fmt, ##__VA_ARGS__)
#else
#define LOGE(fmt, ...) do { } while (0)
#endif

#endif // EINK_LOG_H
