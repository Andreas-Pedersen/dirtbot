#pragma once
#include <Arduino.h>

#define LOG_ENTRIES   25
#define LOG_ENTRY_LEN 80

static char    _logBuf[LOG_ENTRIES][LOG_ENTRY_LEN];
static uint8_t _logHead  = 0;
static uint8_t _logCount = 0;

static void logf(const char* fmt, ...) {
    char buf[LOG_ENTRY_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    strlcpy(_logBuf[_logHead], buf, LOG_ENTRY_LEN);
    _logHead = (_logHead + 1) % LOG_ENTRIES;
    if (_logCount < LOG_ENTRIES) _logCount++;
}

static String buildLogJson() {
    String j = "[";
    uint8_t start = (_logCount < LOG_ENTRIES) ? 0 : _logHead;
    for (uint8_t i = 0; i < _logCount; i++) {
        const char* msg = _logBuf[(start + i) % LOG_ENTRIES];
        if (i) j += ",";
        j += "\"";
        for (const char* p = msg; *p; p++) {
            if      (*p == '"')  j += "\\\"";
            else if (*p == '\\') j += "\\\\";
            else                 j += *p;
        }
        j += "\"";
    }
    return j + "]";
}
