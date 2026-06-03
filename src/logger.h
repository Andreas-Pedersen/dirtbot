#pragma once
#include <ESPAsyncWebServer.h>

extern AsyncEventSource events;

static void logf(const char* fmt, ...) {
    if (events.count() == 0) return;
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    events.send(buf, "log", millis());
}
